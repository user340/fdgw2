/* vim:ai ts=4 sw=4
 * tproxy.c:
 *
 * This is a transparent proxy server. It can be started from inetd to
 * service HTTP requests, or run as a stand-alone daemon. HTTP requests
 * are transparently accepted and passed to the WWW proxy cache for
 * handling. The HTTP request line is modified into a form usable by
 * the WWW proxy cache. The destination WWW site name is extracted from
 * the HTTP headers to avoid DNS lookups.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
# include <syslog.h>
#endif
#ifdef HAVE_PATHS_H
# include <paths.h>
#endif
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifdef IPFILTER
# include <sys/ioctl.h>
# include <netinet/in_systm.h>
# include <netinet/ip.h>
# include <netinet/tcp.h>
# include <net/if.h>
# include <netinet/ip_compat.h>
# include <netinet/ip_fil.h>
# include <netinet/ip_nat.h>
#endif

#ifdef IPTABLES
# include <linux/netfilter_ipv4.h>
#endif

#ifdef TCP_WRAPPERS
# include <tcpd.h>
#endif

/* For Solaris */
#ifndef INADDR_NONE
# define INADDR_NONE		(in_addr_t)-1
#endif

/* For Linux */
#if defined(SIGCLD) && !defined(SIGCHLD)
# define SIGCHLD			SIGCLD
#endif

#ifndef _PATH_DEVNULL
# define _PATH_DEVNULL		"/dev/null"
#endif
#ifndef _PATH_VARRUN
# define _PATH_VARRUN		"/var/run/"
#endif

#include "acl.h"

#define ACTIVITY_TIMEOUT	120			/* 120 seconds, 2 minutes */
#define BUFFER_SIZE			(32769)		/* Requests must fit in this size */
#define REFERRER_SIZE		(1024)
#define HOSTNAME_SIZE		(256)

#define PS_METHOD			0
#define PS_METHOD_END		5
#define PS_URI				10
#define PS_URI_END			15
#define PS_VERSION			20
#define PS_HEADER			30
#define PS_HEADER_END		35
#define PS_POST_DATA		40
#define PS_END_OF_REQUEST	45
#define PS_HOST_HEADER		50	/* Needs 10 slots */
#define PS_REFERER_HEADER	60	/* Needs 10 slots */
#define PS_LENGTH_HEADER	70	/* Needs 20 slots */
#define PS_TRANSPARENT		100

#define RC_TRANSPARENT		1
#define RC_FORCED			2
#define RC_DNSLOOKUP		3
#define RC_NODNS			4

/*
 * Typedefs.
 */
typedef unsigned long	ip_address_t;
typedef unsigned char	byte_t;

typedef struct buffer_
{
	byte_t				buffer[BUFFER_SIZE];
	int					head;
	int					free_space;
}	buffer_t;

typedef struct connection_
{
	int					client_fd;
	int					proxy_fd;
	struct sockaddr_in	client_addr;
	struct sockaddr_in	dest_addr;
	buffer_t			read_buffer;
	buffer_t			request;
	int					parse_state;
	int					method_end_offset;
	int					url_offset;
	int					url_end_offset;
	int					version_offset;
	char				host_header[HOSTNAME_SIZE];
	int					host_header_length;
	char				referer_header[REFERRER_SIZE];
	int					referer_header_length;
	int					content_length;
	int					request_number;
}	connection_t;

/*
 * Macros.
 */
#define FD_MAX(a,b)		((a) > (b) ? (a) : (b))

/*
 * Function prototypes.
 */
static void usage(char *prog, char *opt);
static short getportnum(char *portnum);
static ip_address_t getipaddress(char *ipaddr);
static uid_t getuserid(char *user);
static uid_t getgroupid(uid_t uid);
static int bind_to_port(ip_address_t bind_ip, short bind_port);
static int connect_to_proxy(ip_address_t ip, short port);
static void lookup_hostname(struct sockaddr_in *addr, char *hostname, int hostlen, int needed);
static void server_main_loop(int sock, char *server_hostname, short server_port);
static void trans_proxy(int sock, struct sockaddr_in *addr,
						char *server_hostname, short server_port);
static int process_proxy_response(connection_t *conn, size_t last_read);
static int process_client_request(connection_t *conn, size_t last_read);
static int send_data(int fd, char *data, size_t length);
#if defined(DEBUG_INPUT_REQUESTS) || defined(DEBUG_OUTPUT_REQUESTS)
static void print_request(char *prefix, char *data, size_t length);
#endif
static void write_pid(char *prog);
static void term_signal(int sig);
static void alarm_signal(int sig);
#ifdef LOG_TO_FILE
static void hup_signal(int sig);
static void log_it(char *fmt, ...);
#endif

/*
 * Command line switches.
 */
static char				*prog;
static int				daemonize = 1;
static int				fully_transparent = 0;
static int				proxy_only = 0;
static char				*force_url = NULL;
static int				force_url_length;
#ifdef LOG_TO_FILE
static char				*log_file_name = NULL;
static FILE				*log_file = NULL;
#endif

#ifdef IPFILTER
/*
 * The /dev/ipnat device node.
 */
static int				natdev = -1;
#endif

#ifdef TCP_WRAPPERS
/*
 * The syslog levels for tcp_wrapper checking.
 */
int						deny_severity;
int						allow_severity;
#endif

/*
 * Main loop.
 */
int main(int argc, char **argv)
{
	int					arg;
	int					run_as_server = 0;
	ip_address_t		bind_ip = INADDR_ANY;
	short				bind_port = -1;
	char 				*server_hostname = NULL;
	short				server_port = -1;
	uid_t				run_uid = 0;
	gid_t				run_gid = 0;
#if !defined(DEBUG) && !defined(DEBUG_INPUT_REQUESTS) && !defined(DEBUG_OUTPUT_REQUESTS)
	int					fd;
#endif
	int					sock;
	struct sockaddr_in	addr;
	int					len;

	/*
	 * Get the name if the program.
	 */
	if ((prog = strrchr(argv[0], '/')) != NULL)
		++prog;
	else
		prog = argv[0];

	/*
	 * Reset the access control list.
	 */
	reset_acl();

	/*
	 * Parse the command line arguments.
	 */
	while ((arg = getopt(argc, argv, "dtps:r:b:f:l:a:")) != EOF)
	{
		switch (arg)
		{
		case 't':
			fully_transparent = 1;
			proxy_only = 0;
			break;

		case 'p':
			proxy_only = 1;
			fully_transparent = 0;
			break;

		case 's':
			run_as_server = 1;
			bind_port = getportnum(optarg);
			break;

		case 'd':
			if (run_as_server)
			{
				daemonize = 0;
			}
			else
			{
				usage(prog, "This option is valid when running as a server");
			}
			break;

		case 'b':
			if (run_as_server)
			{
				bind_ip = getipaddress(optarg);
			}
			else
			{
				usage(prog, "This option is valid when running as a server");
			}
			break;

		case 'r':
			if (run_as_server)
			{
				run_uid = getuserid(optarg);
				run_gid = getgroupid(run_uid);
			}
			else
			{
				usage(prog, "This option is valid when running as a server");
			}
			break;

		case 'a':
			if (run_as_server)
			{
				if (!add_acl(optarg))
					usage(prog, "ACL not in expected format");
			}
			else
			{
				usage(prog, "This option is valid when running as a server");
			}
			break;

		case 'f':
			force_url = optarg;
			force_url_length = strlen(force_url);
			break;

#ifdef LOG_TO_FILE
		case 'l':
			log_file_name = optarg;
			break;
#endif

		default:
		case '?':
			usage(prog, NULL);
			break;
		}
	}

	/*
	 * Process the remaining command line arguments.
	 */
	for (; optind < argc; ++optind)
	{
		if (server_hostname == NULL)
		{
			server_hostname = argv[optind];
		}
		else if (server_port == -1)
		{
			server_port = getportnum(argv[optind]);
		}
		else
		{
			usage(prog, "Extra arguments were specified.");
		}
	}

	/*
	 * Test to make sure required args were provided and are valid.
	 */
	if (!fully_transparent && server_hostname == NULL)
	{
		usage(prog, "No proxyhost was specified (or it was invalid).");
	}
	if (!fully_transparent && server_port == -1)
	{
		usage(prog, "No proxyport was specified (or it was invalid).");
	}
	if (run_as_server && (bind_ip == INADDR_NONE))
	{
		usage(prog, "The server ipaddr is invalid.");
	}
	if (run_as_server && (bind_port == -1))
	{
		usage(prog, "No server port was specified (or it was invalid).");
	}

	/*
	 * Set the current directory to the root filesystem.
	 */
	chdir("/");

#ifdef IPFILTER
	/*
	 * Now is a good time to open /dev/ipnat, before giving up our uid/gid.
	 */
	if ((natdev = open(IPL_NAT, O_RDONLY)) < 0)
	{
		perror("open(" IPL_NAT ")");
		exit(1);
	}
#endif

#ifdef LOG_TO_FILE
	/*
	 * Open the log file for the first time.
	 */
	if (log_file_name)
	{
		log_file = fopen(log_file_name, "a");
# ifdef LOG_TO_FILE_LINEBUFF
		if (log_file)
			setvbuf(log_file, NULL, _IOLBF, BUFSIZ);
# endif
	}
#endif

	/*
	 * See if we should run as a server.
	 */
	if (run_as_server)
	{
		/*
		 * Start by binding to the port, the child inherits this socket.
		 */
		sock = bind_to_port(bind_ip, bind_port);

		/*
		 * Start a server process.
		 */
		if (daemonize)
		{
			switch (fork())
			{
			case -1:
				/*
				 * We could not fork a daemon process.
				 */
				perror("fork()");
				exit(1);
			case 0:
				/*
				 * The child continues as the daemon process.
				 */
				break;
			default:
				/*
				 * Parent exits at this stage.
				 */
				_exit(0);
			}

			/*
			 * Start a new session and group.
			 */
			setsid();
		}

#if !defined(DEBUG) && !defined(DEBUG_INPUT_REQUESTS) && !defined(DEBUG_OUTPUT_REQUESTS)
		/*
		 * Point file descriptors to /dev/null, unless DEBUG is defined.
		 */
		if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) >= 0)
		{
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > STDERR_FILENO)
				close(fd);
		}
#endif

#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
		/*
		 * Open syslog for logging errors.
		 */
		openlog(prog, LOG_PID, LOG_DAEMON);
#endif

		/*
		 * Ignore some signals.
		 */
		if (daemonize)
			signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);
		signal(SIGCONT, SIG_IGN);
		signal(SIGPIPE, SIG_IGN);
		signal(SIGALRM, alarm_signal);
#ifdef LOG_TO_FILE
		signal(SIGHUP, hup_signal);
#else
		signal(SIGHUP, SIG_IGN);
#endif
		signal(SIGTERM, term_signal);

		/*
		 * Create a .pid file in /var/run.
		 */
		write_pid(prog);

		/*
		 * Drop back to an untrusted user.
		 */
		if (run_gid != 0)
			setgid(run_gid);
		if (run_uid != 0)
			setuid(run_uid);

		/*
		 * Handle the server main loop.
		 */
		server_main_loop(sock, server_hostname, server_port);

		/*
		 * Should never exit.
		 */
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
		closelog();
#endif
		exit(1);
	}

#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
	/*
	 * Open syslog for logging errors.
	 */
	openlog(prog, LOG_PID, LOG_DAEMON);
#endif

	/*
	 * We are running from inetd so find the peer address.
	 */
	len = sizeof(addr);
	if (getpeername(STDIN_FILENO, (struct sockaddr *)&addr, &len) < 0)
	{
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
		syslog(LOG_ERR, "getpeername(): %m");
		closelog();
#endif
		exit(1);
	}

	/*
	 * Set the keepalive socket option to on.
	 */
	{
		int		on = 1;
		setsockopt(STDIN_FILENO, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on));
	}

	/*
	 * We are running from inetd so process stdin.
	 */
	trans_proxy(STDIN_FILENO, &addr, server_hostname, server_port);
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
	closelog();
#endif

	return (0);
}

/*
 * Print some basic help information.
 */
static void usage(char *prog, char *opt)
{
	if (opt)
	{
		fprintf(stderr, "%s: %s\n", prog, opt);
	}
#ifdef LOG_TO_FILE
	fprintf(stderr, "usage: %s [-t | -p] [-f url] [-s port [-d] [-b ipaddr] [-r user] [-a acl]] [-l file] proxyhost proxyport\n", prog);
#else
	fprintf(stderr, "usage: %s [-t | -p] [-f url] [-s port [-d] [-b ipaddr] [-r user] [-a acl]] proxyhost proxyport\n", prog);
#endif
	fprintf(stderr, "    -t          Operate transparently, don't talk to a proxy.\n");
	fprintf(stderr, "    -p          Only talk to a proxy, disable direct fall-back.\n");
	fprintf(stderr, "    -f url      Force access to always go to specified URL.\n");
	fprintf(stderr, "    -s port     Run as a server bound to the specified port.\n");
	fprintf(stderr, "    -d          Do not background the daemon in server mode.\n");
	fprintf(stderr, "    -b ipaddr   Bind to the specified ipaddr in server mode.\n");
	fprintf(stderr, "    -r user     Run as the specified user in server mode.\n");
	fprintf(stderr, "    -a acl      Add an ACL to restrict connections in server mode.\n");
#ifdef LOG_TO_FILE
	fprintf(stderr, "    -l file     Log accesses to the specified file (relative to /).\n");
#endif
	exit(1);
}

/*
 * Return the port number, in network order, of the specified service.
 */
static short getportnum(char *portnum)
{
	char			*digits = portnum;
	struct servent	*serv;
	short			port;

	for (port = 0; isdigit((int)*digits); ++digits)
	{
		port = (port * 10) + (*digits - '0');
	}

	if ((*digits != '\0') || (port <= 0))
	{
		if ((serv = getservbyname(portnum, "tcp")) != NULL)
		{
			port = serv->s_port;
		}
		else
		{
			port = -1;
		}
		endservent();
	}
	else
		port = htons(port);

#ifdef DEBUG
	printf("Port lookup %s -> %hd\n", portnum, ntohs(port));
#endif

	return (port);
}

/*
 * Return the IP address of the specified host.
 */
static ip_address_t getipaddress(char *ipaddr)
{
	struct hostent	*host;
	ip_address_t	ip;

	if (((ip = inet_addr(ipaddr)) == INADDR_NONE)
	    &&
		(strcmp(ipaddr, "255.255.255.255") != 0))
	{
		if ((host = gethostbyname(ipaddr)) != NULL)
		{
			memcpy(&ip, host->h_addr, sizeof(ip));
		}
		endhostent();
	}

#ifdef DEBUG
	printf("IP lookup %s -> 0x%08lx\n", ipaddr, ntohl(ip));
#endif

	return (ip);
}

/*
 * Find the userid of the specified user.
 */
static uid_t getuserid(char *user)
{
	struct passwd	*pw;
	uid_t			uid;

	if ((pw = getpwnam(user)) != NULL)
	{
		uid = pw->pw_uid;
	}
	else if (*user == '#')
	{
		uid = (uid_t)atoi(&user[1]);
	}
	else
	{
		uid = -1;
	}

#ifdef DEBUG
	printf("User lookup %s -> %lu\n", user, (unsigned long)uid);
#endif

	endpwent();

	return (uid);
}

/*
 * Find the groupid of the specified user.
 */
static uid_t getgroupid(uid_t uid)
{
	struct passwd	*pw;
	gid_t			gid;

	if ((pw = getpwuid(uid)) != NULL)
	{
		gid = pw->pw_gid;
	}
	else
	{
		gid = -1;
	}

#ifdef DEBUG
	printf("Group lookup %lu -> %lu\n", (unsigned long)uid, (unsigned long)gid);
#endif

	endpwent();

	return (gid);
}

/*
 * Bind to the specified ip and port.
 */
static int bind_to_port(ip_address_t bind_ip, short bind_port)
{
	struct sockaddr_in	addr;
	int					sock;

	/*
	 * Allocate a socket.
	 */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket()");
		exit(1);
	}

	/*
	 * Set the SO_REUSEADDR option so we can start tproxy quickly if it dies.
	 */
	{
	 	int	on = 1;

		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
	}

	/*
	 * Set the address to listen to.
	 */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = bind_port;
	addr.sin_addr.s_addr = bind_ip;

	/*
	 * Bind our socket to the above address.
	 */
	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind()");
		close(sock);
		exit(1);
	}

	/*
	 * Establish a large listen backlog.
	 */
	if (listen(sock, SOMAXCONN) < 0)
	{
		perror("listen()");
		close(sock);
		exit(1);
	}

	return (sock);
}

/*
 * Connect to the proxy server.
 */
static int connect_to_proxy(ip_address_t ip, short port)
{
	struct sockaddr_in	addr;
	int					sock;

	/*
	 * Allocate a socket.
	 */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
		syslog(LOG_ERR, "socket(): %m");
#endif
		return (-1);
	}

	/*
	 * Set the address to connect to.
	 */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = ip;

	/*
	 * Connect our socket to the above address.
	 */
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
		syslog(LOG_WARNING, "connect(): %m");
#endif
		close(sock);
		return (-1);
	}

	/*
	 * Set the keepalive socket option to on.
	 */
	{
		int		on = 1;
		setsockopt(STDIN_FILENO, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on));
	}

	return (sock);
}

/*
 * Translate a sockaddr_in structure into a usable ASCII hostname.
 */
static void lookup_hostname(struct sockaddr_in *addr, char *hostname, int hostlen, int needed)
{
#ifdef DNS_LOOKUPS
	struct hostent	*host;

	if (needed)
	{
		if ((host = gethostbyaddr((char *)&addr->sin_addr,
								  sizeof(addr->sin_addr), AF_INET)) != NULL)
		{
			strncpy(hostname, host->h_name, hostlen);
			hostname[hostlen - 1] = '\0';
		}
		else
		{
			strncpy(hostname, inet_ntoa(addr->sin_addr), hostlen);
			hostname[hostlen - 1] = '\0';
# ifdef LOG_TO_SYSLOG
			syslog(LOG_WARNING, "DNS lookup for %s failed: %m", hostname);
# endif
		}
	}
	else
	{
# ifdef USELESS_DNS_LOOKUPS
		if ((host = gethostbyaddr((char *)&addr->sin_addr,
								  sizeof(addr->sin_addr), AF_INET)) != NULL)
		{
			strncpy(hostname, host->h_name, hostlen);
			hostname[hostlen - 1] = '\0';
		}
		else
		{
			strncpy(hostname, inet_ntoa(addr->sin_addr), hostlen);
			hostname[hostlen - 1] = '\0';
#  ifdef LOG_TO_SYSLOG
			syslog(LOG_WARNING, "DNS lookup for %s failed: %m", hostname);
#  endif
		}
# else
		strncpy(hostname, inet_ntoa(addr->sin_addr), hostlen);
		hostname[hostlen - 1] = '\0';
# endif
	}
#else
	strncpy(hostname, inet_ntoa(addr->sin_addr), hostlen);
	hostname[hostlen - 1] = '\0';
#endif
}

/*
 * This is the main loop when running as a server.
 */
static void server_main_loop(int sock, char *server_hostname, short server_port)
{
	int					new_sock;
	struct sockaddr_in	addr;
	int					len;
	pid_t				pid;
#ifdef DO_DOUBLE_FORK
	int					status;
#endif
#ifdef TCP_WRAPPERS
	struct request_info	req;
#endif

	/*
	 * Ignore dead children so no zombies should be left hanging. However some
	 * systems don't allow this, see the DO_DOUBLE_FORK option.
	 */
#ifdef SA_NOCLDWAIT
	/* Go BSD */
	{
		struct sigaction	sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = SIG_IGN;
		sa.sa_flags = SA_NOCLDWAIT;
		sigaction(SIGCHLD, &sa, NULL);
	}
#else
	/* Go Linux & SVR4 */
 	signal(SIGCHLD, SIG_IGN);
#endif

	for (;;)
	{
		/*
		 * Accept an incoming connection.
		 */
		len = sizeof(addr);
		while ((new_sock = accept(sock, (struct sockaddr *)&addr, &len)) < 0)
		{
			/*
			 * Connection resets are common enough to log them as debug only.
			 */
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
			if (errno != ECONNRESET)
				syslog(LOG_ERR, "accept(): %m");
# ifdef LOG_TO_SYSLOG
			else
				syslog(LOG_INFO, "accept(): %m");
# endif
#endif
		}

		/*
		 * Check if the access is allowed by ACL rule.
		 */
		if (!check_acl(&addr))
		{
#ifdef LOG_TO_SYSLOG
			syslog(LOG_AUTH | LOG_NOTICE, "ACL refused from %.100s",
					inet_ntoa(addr.sin_addr));
#endif
#ifdef LOG_TO_FILE
			log_it("ACL refused from %.100s", inet_ntoa(addr.sin_addr));
#endif
		}
		else
		{
#ifdef TCP_WRAPPERS
			/*
			 * Initialise the tcp_wrappers request structure. Get the information
			 * about the remote machine that is connecting to us.
			 */
			request_init(&req, RQ_DAEMON, "tproxy", RQ_FILE, new_sock, 0);
			fromhost(&req);

			/*
			 * Initialise the default logging facility and level. Note that
			 * we only allow for logging refused connections. There is enough
			 * logging for valid connections elsewhere.
			 */
			deny_severity = LOG_AUTH | LOG_NOTICE;

			/*
			 * Determine if access is allowed.
			 */
			if (!hosts_access(&req))
			{
# ifdef LOG_TO_SYSLOG
				syslog(deny_severity, "refused connection from %.100s", eval_client(&req));
# endif
# ifdef LOG_TO_FILE
				log_it("Refused connection from %.100s", eval_client(&req));
# endif
			}
			else
#endif
			{
				/*
				 * Create a new process to handle the connection.
				 */
				switch (pid = fork())
				{
				case -1:	/* Error */
					/*
					 * Under load conditions just ignore new connections.
					 */
					break;

				case 0:		/* Child */
#ifdef DO_DOUBLE_FORK
					/*
					 * Some systems like to fork again as the child. This makes the client
					 * run as a child of init so we don't have to handle reaping them.
					 * However all this forking slows us downm so it's optional.
					 */
					if (fork() > 0)	/* Parent */
						exit(0);
#endif

					/*
					 * Start the proxy work in the new socket.
					 */
					trans_proxy(new_sock, &addr, server_hostname, server_port);
					close(new_sock);
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
					closelog();
#endif
					exit(0);

#ifdef DO_DOUBLE_FORK
				default:	/* Parent */
					/*
					 * Wait for the child process to terminate. Since it forked another
					 * child to handle the request, it should terminate quickly.
					 */
					waitpid(pid, &status, 0);
					break;
#endif
				}
			}
		}

		/*
		 * Close the socket as the child handles the call.
		 */
		close(new_sock);
	}
}

/*
 * Perform the transparent proxy activity.
 */
static void trans_proxy(int sock, struct sockaddr_in *from_addr,
						char *server_hostname, short server_port)
{
	ip_address_t		server_ip;
	connection_t		conn;
	int					length;
	int					max_fd;
	fd_set				read_fd;
#ifdef IPFILTER
	natlookup_t			natlook;
#ifdef __NetBSD__
	natlookup_t			*natlookp;
#endif
#endif

	/*
	 * Initialise the connection structure.
	 */
	memset(&conn, 0, sizeof(connection_t));
	conn.client_fd = sock;
	conn.client_addr = *from_addr;
	conn.read_buffer.free_space = sizeof(conn.read_buffer.buffer);
	conn.request.free_space = sizeof(conn.request.buffer);
	conn.parse_state = PS_METHOD;
	conn.parse_state = PS_METHOD;

#ifdef IPTABLES
	/*
	 * The first thing we do is get the IP address that the client was
	 * trying to connected to. Here lies part of the magic. Linux-2.4
	 * introduces IPTABLES which uses getsockopt instead of getsockname
	 * for a more obvious interface.
	 */
	length = sizeof(conn.dest_addr);
	if (getsockopt(sock, SOL_IP, SO_ORIGINAL_DST, (char *)&conn.dest_addr, &length) < 0)
	{
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
		syslog(LOG_ERR, "getsockopt(SO_ORIGINAL_DST): %m");
#endif
		return;
	}
	
#else/*!IPTABLES*/

	/*
	 * The first thing we do is get the IP address that the client was
	 * trying to connected to. Here lies part of the magic. Normally
	 * getsockname returns our address, but not with transparent proxying.
	 */
	length = sizeof(conn.dest_addr);
	if (getsockname(sock, (struct sockaddr *)&conn.dest_addr, &length) < 0)
	{
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
		syslog(LOG_ERR, "getsockname(): %m");
#endif
		return;
	}

#ifdef IPFILTER
	/*
	 * However, under IPFILTER, we have to do additional work. The "to_addr"
	 * has been changed by the IP filter to trick the IP stack into
	 * processing the packet locally. We need to lookup the real remote
	 * address ourselves using an ioctl().
	 */

	/*
	 * Build up the NAT natlookup structure.
	 */
	memset(&natlook, 0, sizeof(natlook));
	natlook.nl_inip = conn.dest_addr.sin_addr;
	natlook.nl_outip = conn.client_addr.sin_addr;
	natlook.nl_flags = IPN_TCP;
	natlook.nl_inport = conn.dest_addr.sin_port;
	natlook.nl_outport = conn.client_addr.sin_port;
#ifdef __NetBSD__
        natlookp = &natlook;
#endif

	/*
	 * Use the NAT device to lookup the mapping pair.
	 */
#ifdef __NetBSD__
	if (ioctl(natdev, SIOCGNATL, &natlookp) == -1)
#else
        if (ioctl(natdev, SIOCGNATL, &natlook) == -1)
#endif
	{
# if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
		syslog(LOG_ERR, "ioctl(SIOCGNATL): %m");
# endif
		return;
	}

#ifdef __NetBSD__
	conn.dest_addr.sin_addr = natlookp->nl_realip;
	conn.dest_addr.sin_port = natlookp->nl_realport;
#else
	conn.dest_addr.sin_addr = natlook.nl_realip;
	conn.dest_addr.sin_port = natlook.nl_realport;
#endif
#endif

#endif/*!IPTABLES*/

	/*
	 * Lookup proxy server IP address. Because we do the lookup for every
	 * request we make use of DNS round-robin for load balancing.
	 */
	server_ip = getipaddress(server_hostname);
	if (server_ip == INADDR_NONE)
	{
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
		syslog(LOG_ERR, "Could not resolve IP address for proxy: %s", server_hostname);
#endif
		return;
	}

	/*
	 * Connect to the proxy server (or to the original destination in the case
	 * of fully transparent mode, or a failed connect to the proxy).
	 */
	if (fully_transparent ||
		(conn.proxy_fd = connect_to_proxy(server_ip, server_port)) == -1)
	{
		if (!proxy_only)
		{
			conn.proxy_fd = connect_to_proxy(conn.dest_addr.sin_addr.s_addr, conn.dest_addr.sin_port);
			fully_transparent = 1;
		}
		else
		{
			conn.proxy_fd = -1;
		}
	}
	if (conn.proxy_fd == -1)
	{
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
		if (fully_transparent)
			syslog(LOG_ERR, "Could not connect to destination");
		else if (proxy_only)
			syslog(LOG_ERR, "Could not connect to proxy host \"%s\"", server_hostname);
		else
			syslog(LOG_ERR, "Could not connect to proxy host \"%s\" or destination", server_hostname);
#endif
		return;
	}

	/*
	 * This loop acts a bit like the guy in the middle of a "bucket brigade".
	 * When the client passes some data, it gets handed off to the server,
	 * and vise-versa. However for data received from the client, it is
	 * buffered until a full request is received. Once a full request is
	 * received, the request is re-written before being sent to the server.
	 */
	for (;;)
	{
		/*
		 * Construct a select read mask from both file descriptors.
		 */
		FD_ZERO(&read_fd);
		FD_SET(conn.client_fd, &read_fd);
		FD_SET(conn.proxy_fd, &read_fd);
		max_fd = FD_MAX(conn.client_fd, conn.proxy_fd);

		/*
		 * Disconnect after some period of in-activity.
		 */
		alarm(ACTIVITY_TIMEOUT);

		/*
		 * Wait for some data to be read. If we get an signal then
		 * ignore the -1 return and try again.
		 */
		if (select(max_fd + 1, &read_fd, NULL, NULL, NULL) < 0)
		{
			if (errno != EINTR)
			{
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
				syslog(LOG_ERR, "select(): %m");
#endif
				close(conn.proxy_fd);
				return;
			}
		}

		/*
		 * We have something so disable the alarm.
		 */
		alarm(0);

		/*
		 * See if any data can be read from the client.
		 */
		if (FD_ISSET(conn.client_fd, &read_fd))
		{
			switch (length = read(conn.client_fd,
					&conn.request.buffer[conn.request.head],
					conn.request.free_space - conn.request.head))
			{
			case -1:
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
				if (errno != ECONNRESET)
					syslog(LOG_WARNING, "read(client) failed: %m");
# ifdef LOG_TO_SYSLOG
				else
					syslog(LOG_INFO, "read(client) failed: %m");
# endif
#endif
				close(conn.proxy_fd);
				return;

			case 0:
				close(conn.proxy_fd);
				return;

			default:
				/*
				 * Take into account this new data chunk.
				 */
				conn.request.head += length;

				/*
				 * Process requests read from the client. Returns 0 if a
				 * processed request could not be written to the proxy.
				 */
				if (!process_client_request(&conn, length))
				{
#ifdef LOG_TO_SYSLOG
					syslog(LOG_WARNING, "write(proxy) failed: %m");
#endif
					close(conn.proxy_fd);
					return;
				}
				break;
			}
		}

		/*
		 * See if any data can be read from the proxy.
		 */
		if (FD_ISSET(conn.proxy_fd, &read_fd))
		{
			switch (length = read(conn.proxy_fd,
					&conn.read_buffer.buffer[conn.read_buffer.head],
					conn.read_buffer.free_space - conn.read_buffer.head))
			{
			case -1:
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
				syslog(LOG_WARNING, "read(proxy) failed: %m");
#endif
				close(conn.proxy_fd);
				return;

			case 0:
				close(conn.proxy_fd);
				return;

			default:
				/*
				 * Take into account this new data chunk.
				 */
				conn.read_buffer.head += length;

				/*
				 * Process responses read from the proxy. Returns 0 if a
				 * processed request could not be written to the client.
				 */
				if (!process_proxy_response(&conn, length))
				{
#ifdef LOG_TO_SYSLOG
					syslog(LOG_WARNING, "write(client) failed: %m");
#endif
					close(conn.proxy_fd);
					return;
				}
				break;
			}
		}
	}
}

/*
 * Process responses read from the proxy. Returns 0 if a
 * processed request could not be written to the client.
 */
static int process_proxy_response(connection_t *conn, size_t last_read)
{
	/*
	 * Write the response to the client.
	 */
	if (!send_data(conn->client_fd, conn->read_buffer.buffer, conn->read_buffer.head))
		return (0);

	/*
	 * Now that all data is written, reset the "head" index.
	 */
	conn->read_buffer.head = 0;

	return (1);
}

/*
 * Process requests read from the client. Returns 0 if a
 * processed request could not be written to the proxy.
 *
 * Requests are parsed by way of a state machine. The
 * request is only written to the proxy when it has been
 * fully read and processed.
 */
static int process_client_request(connection_t *conn, size_t last_read)
{
	size_t	index;
	size_t	send_size;
	int		c;
	int		request_change;
#if defined(LOG_TO_SYSLOG) || defined(LOG_TO_FILE)
	char	client_host[HOSTNAME_SIZE];
#endif
	char	request_buffer[BUFFER_SIZE];
	char	request_port[16];
#if defined(LOG_TO_SYSLOG) || defined(LOG_TO_FILE)
	char	*url_string;
#endif

	/*
	 * Start the processing over again. Called when switching to
	 * transparent mode after a CONNECT method is received.
	 */
reprocess_buffer:

	/*
	 * Transparent mode simply sends any read data to the proxy.
	 */
	if (conn->parse_state == PS_TRANSPARENT)
	{
		/*
		 * Write the buffer to the proxy.
		 */
		if (!send_data(conn->proxy_fd, conn->request.buffer, conn->request.head))
			return (0);

		conn->request.head = 0;
	}
	else
	{
		/*
		 * Loop over the data that has just been added to the request buffer.
		 */
		index = conn->request.head - last_read;
		while (index < conn->request.head)
		{
			/*
			 * Fetch the next character to process.
			 */
			c = conn->request.buffer[index];

			/*
			 * Parse the data that we currently have. This state machine handles HTTP/0.9
			 * requests that don't include headers, as well as HTTP/1.x requests that
			 * do include headers and are terminated with a blank line.
			 */
			switch (conn->parse_state)
			{
			case PS_METHOD:
				if (c == '\r')					conn->parse_state = PS_METHOD + 1;
				else if (c == '\n')				conn->parse_state = PS_END_OF_REQUEST;
				else if (c == ' ') {			conn->parse_state = PS_METHOD_END;
					conn->method_end_offset = index;
				}
				break;

			case PS_METHOD + 1:
				if (c == '\n')					conn->parse_state = PS_END_OF_REQUEST;
				else							conn->parse_state = PS_METHOD;
				break;

			case PS_METHOD_END:
				if (c == '\r')					conn->parse_state = PS_METHOD_END + 1;
				else if (c == '\n')				conn->parse_state = PS_END_OF_REQUEST;
				else if (c != ' ') {			conn->parse_state = PS_URI;
					conn->url_offset = index;
				}
				break;

			case PS_METHOD_END + 1:
				if (c == '\n')					conn->parse_state = PS_END_OF_REQUEST;
				else							conn->parse_state = PS_METHOD_END;
				break;

			case PS_URI:
				if (c == '\r') {				conn->parse_state = PS_URI + 1;
					conn->url_end_offset = index;
				}
				else if (c == '\n') {			conn->parse_state = PS_END_OF_REQUEST;
					conn->url_end_offset = index;
				}
				else if (c == ' ') {			conn->parse_state = PS_URI_END;
					conn->url_end_offset = index;
				}
				break;

			case PS_URI + 1:
				if (c == '\n')					conn->parse_state = PS_END_OF_REQUEST;
				else							conn->parse_state = PS_URI;
				break;

			case PS_URI_END:
				if (c == '\r')					conn->parse_state = PS_URI_END + 1;
				else if (c == '\n')				conn->parse_state = PS_END_OF_REQUEST;
				else if (c != ' ') {			conn->parse_state = PS_VERSION;
					conn->version_offset = index;
				}
				break;

			case PS_URI_END + 1:
				if (c == '\n')					conn->parse_state = PS_END_OF_REQUEST;
				else							conn->parse_state = PS_URI_END;
				break;

			case PS_VERSION:
				if (c == '\r')					conn->parse_state = PS_VERSION + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				break;

			case PS_VERSION + 1:
				if (c == '\n')					conn->parse_state = PS_HEADER_END;
				else							conn->parse_state = PS_VERSION;
				break;

			case PS_HEADER:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				break;

			case PS_HEADER + 1:
				if (c == '\n')					conn->parse_state = PS_HEADER_END;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_HEADER_END:
				if (c == '\r')					conn->parse_state = PS_HEADER_END + 1;
				else if (c == '\n') {
					if (conn->content_length > 0)
						conn->parse_state = PS_POST_DATA;
					else
						conn->parse_state = PS_END_OF_REQUEST;
				}
				else if (c == 'H' || c == 'h')	conn->parse_state = PS_HOST_HEADER;
				else if (c == 'R' || c == 'r')	conn->parse_state = PS_REFERER_HEADER;
				else if (c == 'C' || c == 'c')	conn->parse_state = PS_LENGTH_HEADER;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_HEADER_END + 1:
				if (c == '\n') {
					if (conn->content_length > 0)
						conn->parse_state = PS_POST_DATA;
					else
						conn->parse_state = PS_END_OF_REQUEST;
				}
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_HOST_HEADER:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'O' || c == 'o')	conn->parse_state = PS_HOST_HEADER + 1;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_HOST_HEADER + 1:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'S' || c == 's')	conn->parse_state = PS_HOST_HEADER + 2;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_HOST_HEADER + 2:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'T' || c == 't')	conn->parse_state = PS_HOST_HEADER + 3;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_HOST_HEADER + 3:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == ':')				conn->parse_state = PS_HOST_HEADER + 4;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_HOST_HEADER + 4:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c != ' ') {			conn->parse_state = PS_HOST_HEADER + 5;
					conn->host_header[conn->host_header_length++] = c;
				}
				break;

			case PS_HOST_HEADER + 5:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (conn->host_header_length < (sizeof(conn->host_header) - 1))
					conn->host_header[conn->host_header_length++] = c;
				break;

			case PS_REFERER_HEADER:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'E' || c == 'e')	conn->parse_state = PS_REFERER_HEADER + 1;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_REFERER_HEADER + 1:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'F' || c == 'f')	conn->parse_state = PS_REFERER_HEADER + 2;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_REFERER_HEADER + 2:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'E' || c == 'e')	conn->parse_state = PS_REFERER_HEADER + 3;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_REFERER_HEADER + 3:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'R' || c == 'r')	conn->parse_state = PS_REFERER_HEADER + 4;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_REFERER_HEADER + 4:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'E' || c == 'e')	conn->parse_state = PS_REFERER_HEADER + 5;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_REFERER_HEADER + 5:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'R' || c == 'r')	conn->parse_state = PS_REFERER_HEADER + 6;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_REFERER_HEADER + 6:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == ':')				conn->parse_state = PS_REFERER_HEADER + 7;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_REFERER_HEADER + 7:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c != ' ') {			conn->parse_state = PS_REFERER_HEADER + 8;
					conn->referer_header[conn->referer_header_length++] = c;
				}
				break;

			case PS_REFERER_HEADER + 8:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (conn->referer_header_length < (sizeof(conn->referer_header) - 1))
					conn->referer_header[conn->referer_header_length++] = c;
				break;

			case PS_LENGTH_HEADER:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'O' || c == 'o')	conn->parse_state = PS_LENGTH_HEADER + 1;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 1:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'N' || c == 'n')	conn->parse_state = PS_LENGTH_HEADER + 2;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 2:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'T' || c == 't')	conn->parse_state = PS_LENGTH_HEADER + 3;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 3:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'E' || c == 'e')	conn->parse_state = PS_LENGTH_HEADER + 4;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 4:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'N' || c == 'n')	conn->parse_state = PS_LENGTH_HEADER + 5;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 5:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'T' || c == 't')	conn->parse_state = PS_LENGTH_HEADER + 6;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 6:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == '-')				conn->parse_state = PS_LENGTH_HEADER + 7;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 7:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'L' || c == 'l')	conn->parse_state = PS_LENGTH_HEADER + 8;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 8:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'E' || c == 'e')	conn->parse_state = PS_LENGTH_HEADER + 9;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 9:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'N' || c == 'n')	conn->parse_state = PS_LENGTH_HEADER + 10;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 10:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'G' || c == 'g')	conn->parse_state = PS_LENGTH_HEADER + 11;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 11:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'T' || c == 't')	conn->parse_state = PS_LENGTH_HEADER + 12;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 12:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == 'H' || c == 'h')	conn->parse_state = PS_LENGTH_HEADER + 13;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 13:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c == ':')				conn->parse_state = PS_LENGTH_HEADER + 14;
				else							conn->parse_state = PS_HEADER;
				break;

			case PS_LENGTH_HEADER + 14:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c != ' ') {			conn->parse_state = PS_LENGTH_HEADER + 15;
					if (c >= '0' && c <= '9')
						conn->content_length = c - '0';
					else
						conn->content_length = 0;
				}
				break;

			case PS_LENGTH_HEADER + 15:
				if (c == '\r')					conn->parse_state = PS_HEADER + 1;
				else if (c == '\n')				conn->parse_state = PS_HEADER_END;
				else if (c >= '0' && c <= '9') {
					conn->content_length *= 10;
					conn->content_length += c - '0';
				}
				break;

			case PS_POST_DATA:
				if (conn->content_length <= 1)	conn->parse_state = PS_END_OF_REQUEST;
				else
					--conn->content_length;
				break;

			case PS_TRANSPARENT:
				break;
			}

			/*
			 * We have now officially "consumed" the character.
			 */
			++index;

#ifdef DEBUG
			/*
			 * Print the current character and the parse state.
			 */
			if (c >= ' ' && c <= '~')
				printf("Char:    %c, parse state %d\n", c, conn->parse_state);
			else
				printf("Char: 0x%02x, parse state %d\n", c, conn->parse_state);
#endif

			/*
			 * OK we have parsed a complete request.
			 */
			if (conn->parse_state == PS_END_OF_REQUEST)
			{
				/*
				 * Keep track of how many requests per connection.
				 */
				++conn->request_number;

				/*
				 * If we didn't find a Host: header then do a DNS lookup.
				 */
				if (!fully_transparent && conn->host_header_length == 0)
				{
					lookup_hostname(&conn->dest_addr, conn->host_header,
							sizeof(conn->host_header), 1);
					conn->host_header_length = strlen(conn->host_header);

					request_change = RC_DNSLOOKUP;
				}
				else
				{
					conn->host_header[conn->host_header_length] = '\0';

					request_change = RC_NODNS;
				}

#ifdef DEBUG_INPUT_REQUESTS
				/*
				 * For debugging dump out the original request.
				 */
				print_request("Got  : ", conn->request.buffer, index);
#endif

				/*
				 * Build the resulting request into "request_buffer" and set "send_size"
				 * to the length of the request. This enables the final request to be
				 * written with a single write(2) system call. This should be more
				 * efficient for both transproxy and the proxy.
				 */
				if ((fully_transparent) ||
					(strncasecmp(conn->request.buffer, "CONNECT ", 8) == 0))
				{
					/*
					 * Leave the request as it is. Transparent mode is usefull for
					 * operation in a proxy firewall. Or HTTPS protocol doesn't
					 * like modifications so just pass the request on.
					 */
					memcpy(request_buffer, conn->request.buffer, index);
					send_size = index;

					request_change = RC_TRANSPARENT;
				}
				else if (force_url && conn->url_offset > 0)
				{
					/*
					 * Check if the request was referred to by the forced URL.
					 */
					if ((conn->referer_header_length > 0) &&
						(strcmp(conn->referer_header, force_url) == 0))
					{
						/*
						 * It appears that the request is for a page that is
						 * referred to by the original forced URL. In this case
						 * we must let it pass un-changed.
						 */
						memcpy(request_buffer, conn->request.buffer, index);
						send_size = index;
					}
					else
					{
						/*
						 * Force the request to the configured URL. Any images referred to
						 * by this forced URL will work because of the previous check for
						 * requests that have been referred to by the forced URL.
						 */
						send_size = 0;
						memcpy(&request_buffer[send_size], conn->request.buffer, conn->url_offset);
						send_size += conn->url_offset;
						memcpy(&request_buffer[send_size], force_url, force_url_length);
						send_size += force_url_length;
						memcpy(&request_buffer[send_size], &conn->request.buffer[conn->url_end_offset], index - conn->url_end_offset);
						send_size += index - conn->url_end_offset;
						conn->url_end_offset = conn->url_offset + force_url_length;
					}

					request_change = RC_FORCED;
				}
				else if (conn->url_offset > 0 &&
						 conn->request.buffer[conn->url_offset] == '/')
				{
					/*
					 * The request was provided with a URI so we must re-write the request
					 * to a URL of the form {http://hostname}/URI.
					 */
					send_size = 0;
					memcpy(&request_buffer[send_size], conn->request.buffer, conn->url_offset);
					send_size += conn->url_offset;
					memcpy(&request_buffer[send_size], "http://", 7);
					send_size += 7;
					conn->url_end_offset += 7;
					memcpy(&request_buffer[send_size], conn->host_header, conn->host_header_length);
					send_size += conn->host_header_length;
					conn->url_end_offset += conn->host_header_length;
					if ((ntohs(conn->dest_addr.sin_port) != 80) &&
#ifdef USE_STRSTR_BUG
						(strstr(conn->host_header, ":") == NULL))
#else
						(strchr(conn->host_header, ':') == NULL))
#endif
					{
						sprintf(request_port, ":%u", ntohs(conn->dest_addr.sin_port));
						memcpy(&request_buffer[send_size], request_port, strlen(request_port));
						send_size += strlen(request_port);
						conn->url_end_offset += strlen(request_port);
					}
					memcpy(&request_buffer[send_size], &conn->request.buffer[conn->url_offset], index - conn->url_offset);
					send_size += index - conn->url_offset;
				}
				else
				{
					/*
					 * The request didn't appear to have a URI or URL specified. so
					 * leave the request as it is.
					 */
					memcpy(request_buffer, conn->request.buffer, index);
					send_size = index;

					request_change = RC_TRANSPARENT;
				}

				/*
				 * Write the request to the proxy.
				 */
				if (!send_data(conn->proxy_fd, request_buffer, send_size))
					return (0);

#ifdef DEBUG_OUTPUT_REQUESTS
				/*
				 * For debugging dump out the new request.
				 */
				print_request("Sent : ", request_buffer, send_size);
#endif

#if defined(LOG_TO_SYSLOG) || defined(LOG_TO_FILE)
				/*
				 * Find a pretty name for the client.
				 */
				lookup_hostname(&conn->client_addr, client_host, sizeof(client_host), 0);

				/*
				 * Get the URL for logging.
				 */
				if (conn->url_offset > 0)
				{
					url_string = &request_buffer[conn->url_offset];
					request_buffer[conn->url_end_offset] = '\0';
				}
				else
					url_string = "-";

				/*
				 * Log the request.
				 */
				switch (request_change)
				{
				case RC_TRANSPARENT:
# ifdef LOG_TO_SYSLOG
					syslog(LOG_NOTICE, "Transparent   %d %s -> %s",
							conn->request_number, client_host, url_string);
# endif
# ifdef LOG_TO_FILE
					log_it("Transparent   %d %s -> %s",
							conn->request_number, client_host, url_string);
# endif
					break;

				case RC_FORCED:
# ifdef LOG_TO_SYSLOG
					syslog(LOG_NOTICE, "Forced        %d %s -> %s",
							conn->request_number, client_host, url_string);
# endif
# ifdef LOG_TO_FILE
					log_it("Forced        %d %s -> %s",
							conn->request_number, client_host, url_string);
# endif
					break;

				case RC_DNSLOOKUP:
# ifdef LOG_TO_SYSLOG
					syslog(LOG_NOTICE, "Request       %d %s -> %s",
							conn->request_number, client_host, url_string);
# endif
# ifdef LOG_TO_FILE
					log_it("Request       %d %s -> %s",
							conn->request_number, client_host, url_string);
# endif
					break;

				case RC_NODNS:
# ifdef LOG_TO_SYSLOG
					syslog(LOG_NOTICE, "Request_NoDNS %d %s -> %s",
							conn->request_number, client_host, url_string);
# endif
# ifdef LOG_TO_FILE
					log_it("Request_NoDNS %d %s -> %s",
							conn->request_number, client_host, url_string);
# endif
					break;
				}
#endif

				/*
				 * If there is any more data after the current request,
				 * then shift it down in the buffer. Then continue with
				 * the next request.
				 */
				if (index < conn->request.head)
				{
					memcpy(conn->request.buffer, &conn->request.buffer[index],
							conn->request.head - index);
					conn->request.head -= index;
				}
				else
					conn->request.head = 0;

				/*
				 * It we received a CONNECT method then we break out of
				 * the request parsing mode and simply pass data to the
				 * proxy transparently.
				 */
				if (strncasecmp(request_buffer, "CONNECT ", 8) == 0)
				{
					/*
					 * Switch the parser state to transparent mode.
					 */
					conn->parse_state = PS_TRANSPARENT;

					/*
					 * Check if there is any data in the buffer that
					 * should be sent now in transparent mode.
					 */
					goto reprocess_buffer;
				}
				else
				{
					/*
					 * Re-initialise the parser state.
					 */
					conn->parse_state = PS_METHOD;
					conn->url_offset = 0;
					conn->version_offset = 0;
					conn->host_header[0] = '\0';
					conn->host_header_length = 0;
					conn->referer_header_length = 0;
					conn->content_length = 0;
				}
			}
		}
	}

	return (1);
}

/*
 * Send data to a file descriptor (sock). Re-try if the write returns
 * after not sending all of the data.
 */
static int send_data(int fd, char *data, size_t length)
{
	size_t	index;
	size_t	written;

	/*
	 * Loop while there is data to process.
	 */
	for (index = 0; index < length; index += written)
	{
		written = write(fd, &data[index], length - index);

		if (written < 0)
			return (0);
	}

	return (1);
}

#if defined(DEBUG_INPUT_REQUESTS) || defined(DEBUG_OUTPUT_REQUESTS)
/*
 * Print an HTTP request.
 */
static void print_request(char *prefix, char *data, size_t length)
{
	size_t	index;
	int		c;
	int		print_prefix = 1;

	for (index = 0; index < length; ++index)
	{
		if (print_prefix)
			printf("%s", prefix);

		c = data[index];

		if (c >= ' ' && c <= '~')
			printf("%c", c);
		else if (c == '\r')
			printf("\\r");
		else if (c == '\n')
			printf("\\n\n");
		else
			printf(".");

		print_prefix = (c == '\n');
	}

	if (!print_prefix)
		printf("\n");
}
#endif

/*
 * Write out a pid file.
 */
static void write_pid(char *prog)
{
	char	filename[1024];
	FILE	*fp;

	sprintf(filename, "%s%s.pid", _PATH_VARRUN, prog);
	if ((fp = fopen(filename, "w")) != NULL)
	{
		fprintf(fp, "%lu\n", (unsigned long)getpid());
		fclose(fp);
	}
}

/*
 * Catch alarm signals and exit.
 */
static void alarm_signal(int sig)
{
#if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
	syslog(LOG_NOTICE, "Alarm signal caught - connection timeout");
#endif
	exit(1);
}

/*
 * Catch termination signal and exit.
 */
static void term_signal(int sig)
{
	char	filename[1024];

	sprintf(filename, "%s%s.pid", _PATH_VARRUN, prog);
	unlink(filename);

#ifdef LOG_TO_SYSLOG
	syslog(LOG_NOTICE, "Termination signal caught - exiting");
#endif
	exit(1);
}

#ifdef LOG_TO_FILE
/*
 * Catch HUP signals and re-open the log file.
 */
static void hup_signal(int sig)
{
	signal(sig, hup_signal);

	/*
	 * Close the current log file.
	 */
	if (log_file)
	{
		fclose(log_file);
		log_file = NULL;
	}

	/*
	 * Re-open the log file if it is closed.
	 */
	if (log_file == NULL && log_file_name)
	{
		log_file = fopen(log_file_name, "a");
# ifdef LOG_TO_FILE_LINEBUFF
		if (log_file)
			setvbuf(log_file, NULL, _IOLBF, BUFSIZ);
# endif
	}

# if defined(LOG_TO_SYSLOG) || defined(LOG_FAULTS_TO_SYSLOG)
	if (log_file)
		syslog(LOG_NOTICE, "HUP signal caught - re-open log '%s'", log_file_name);
	else if (log_file_name)
		syslog(LOG_NOTICE, "HUP signal caught - re-open failed '%s'", log_file_name);
# endif
}

/*
 * Append an entry to the log file.
 */
static void log_it(char *fmt, ...)
{
	time_t	now;
	char	date[20];
	va_list	args;

	if (log_file)
	{
		time(&now);
		strftime(date, sizeof(date), "%Y/%m/%d %H:%M:%S", localtime(&now));
		fprintf(log_file, "%s  ", date);
		va_start(args, fmt);
		vfprintf(log_file, fmt, args);
		va_end(args);
		fprintf(log_file, "\n");
	}
}
#endif
