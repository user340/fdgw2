
			    Simple Repeater

			   stone version 2.1d

		Copyright(c)1995-2001 by Hiroaki Sengoku
			    sengoku@gcd.org


  stone は、アプリケーションレベルの TCP & UDP パケットリピーターです。
ファイアウォールの内から外へ、あるいは外から内へ、TCP パケットあるいは 
UDP パケットを中継します。

  stone には以下のような特徴があります。

1. Win32 に対応している
	以前は UNIX マシンで構成されることが多かったファイアウォールです
	が、最近は WindowsNT が使われるケースが増えてきました。stone は 
	WindowsNT あるいは Windows95 上で手軽に実行することができます。
	もちろん、Linux, FreeBSD, BSD/OS, SunOS, Solaris, HP-UX などの 
	UNIX マシンでも使うことができます。

2. 単純
	わずか 3000 行 (C 言語) ですので、セキュリティホールが生じる可能
	性を最小限にできます。

3. SSL 対応
	OpenSSL (http://www.openssl.org/) を使うことにより、暗号化/復号
	化してパケットを中継できます。

4. http proxy
	簡易型 http proxy としても使うことができます。

5. POP -> APOP 変換
	APOP に対応していないメーラと stone を使うことで、APOP サーバへ
	アクセスできます。


使用方法

	stone [-d] [-n] [-u <max>] [-f <n>] [-a <file>] [-L <file>] [-l]
	      [-o <n>] [-g <n>] [-t <dir>] [-z <SSL>]
	      [-C <file>] [-P <command>]
	      <st> [-- <st>]...

	オプションとして -d を指定すると、デバッグレベルを増加させます。 
	-z は、SSL 暗号化のオプションです。-n を指定すると、ホスト名や
	サービス名の代わりに IP アドレスやサービス番号を表示します。-u 
	オプションは同時に記憶できる UDP パケットの発信元の最大数を指定
	します。デフォルトは 10 です。-f オプションは子プロセスの数を指
	定します。デフォルトは子プロセス無しです。-a を指定すると、アク
	セスログを file へ出力します。-L を指定すると、エラーメッセージ
	等を file へ出力します。-l を指定すると、エラーメッセージ等を 
	syslog へ出力します。-o と -g はそれぞれユーザ ID とグループ ID 
	を指定します。-t を指定すると、dir へ chroot します。-C はオプショ
	ンおよび <st> をコマンドラインで指定するかわりに設定ファイルから
	読み込みます。-P は設定ファイルを読み込む際のプリプロセッサを指
	定します。

	<st> は次のいずれかです。<st> は「--」で区切ることにより、複数個
	指定できます。

	(1)	<host>:<port> <sport> [<xhost>...]
	(2)	<host>:<port> <shost>:<sport> [<xhost>...]
	(3)	<display> [<xhost>...]
	(4)	proxy <sport> [<xhost>...]
	(5)	<host>:<port>/http <request> [<hosts>...]
	(6)	<host>:<port>/proxy <header> [<hosts>...]

	stone を実行しているマシンのポート <sport> への接続を、他のマシ
	ン <host> のポート <port> へ中継します。インタフェースを複数持つ
	マシンでは、(2) のようにインタフェースのアドレス <shost> を指定
	することにより、特定のインタフェースへの接続のみを転送することが
	できます。

	(3) は、X プロトコル中継のための省略記法です。ディスプレイ番号 
	<display> への接続を、環境変数 DISPLAY で指定した X サーバへ転送
	します。

	(4) は、http proxy です。WWW ブラウザの http proxy の設定で、
	stone を実行しているマシンおよびポート <sport> を指定します。

	(5) は、http リクエストにのせてパケットを中継します。<request> 
	は HTTP 1.0 で規定されるリクエストです。

	(6) は、http リクエストヘッダの先頭に <header> を追加して中継し
	ます。

	<xhost> を列挙することにより、stone へ接続可能なマシンを制限する
	ことができます。マシン名、あるいはその IP アドレスを空白で区切っ
	て指定すると、そのマシンからの接続のみを中継します。

	<xhost> に「/<mask>」を付けると、特定のネットワークのマシンから
	の接続を許可することができます。例えば、クラス C のネットワーク 
	192.168.1.0 の場合は、「192.168.1.0/255.255.255.0」と指定します。

	<sport> に「/udp」を付けると、TCP パケットを中継する代わりに、
	UDP パケットを中継します。

	<port> に「/apop」を付けると、POP を APOP へ変換して中継します。
	変換には RSA Data Security 社の MD5 Message-Digest アルゴリズム
	を使用します。

	<port> に「/ssl」を付けると、パケットを SSL で暗号化して中継します。

	<sport> に「/ssl」を付けると、SSL で暗号化されたパケットを復号化
	して中継します。

	<port> に「/base」を付けると、パケットを MIME base64 で符号化し
	て中継します。

	<sport> に「/base」を付けると、MIME base64 で符号化されたパケッ
	トを復号化して中継します。

	<sport> に「/http」を付けると、http リクエスト上のパケットを中継
	します。


例
	outer: ファイアウォールの外側にあるマシン
	inner: ファイアウォールの内側にあるマシン
	fwall: ファイアウォール. このマシン上で stone を実行

	stone 7 outer
		DISPLAY で指定した X server へ X プロトコルを中継
		outer で DISPLAY=inner:7 と設定して X クライアントを実行

	stone outer:telnet 10023
		outer へ telnet プロトコルを中継
		inner で telnet fwall 10023 を実行

	stone outer:domain/udp domain/udp
		DNS 問い合わせを outer へ中継
		inner で nslookup - fwall を実行

	stone outer:ntp/udp ntp/udp
		outer へ NTP を中継
		inner で ntpdate fwall を実行

	stone localhost:http 443/ssl
		WWW サーバを https 対応にする
		WWW ブラウザで https://fwall/ をアクセス

	    注意: 輸出版 Netscape Navigator 等、多くの WWW ブラウザは 
		  512bit 超の鍵を扱えません

	stone localhost:telnet 10023/ssl
		telnet を SSL 化
		inner で SSLtelnet -z ssl fwall 10023 を実行

	stone proxy 8080
		http proxy

	stone outer:pop/apop pop
		APOP に対応していないメーラで inner:pop へ接続

	fwall が http proxy (port 8080) である時:

	stone fwall:8080/http 10023 'POST http://outer:8023 HTTP/1.0'
	stone localhost:telnet 8023/http
		inner と outer でそれぞれ stone を実行
		http 上でパケットを中継

	stone fwall:8080/proxy 9080 'Proxy-Authorization: Basic c2VuZ29rdTpoaXJvYWtp'
		proxy 認証に対応していないブラウザ用


ホームページ

	stone の公式ホームページは次の URL です。
	http://www.gcd.org/sengoku/stone/Welcome.ja.html


著作権

	この stone に関する全ての著作権は、原著作者である仙石浩明が所有
	します。この stone は、GNU General Public License (GPL) に準ずる
	フリーソフトウェアです。個人的に使用する場合は、改変・複製に制限
	はありません。配布する場合は GPL に従って下さい。


無保証

	この stone は無保証です。この stone を使って生じたいかなる損害に
	対しても、原著作者は責任を負いません。詳しくは GPL を参照して下
	さい。


#2939								仙石 浩明
http://www.gcd.org/sengoku/		Hiroaki Sengoku <sengoku@gcd.org>
