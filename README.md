# slangio
Initially designed to run COBOL applications with a web interface, without modification. I do know of some propriatary solutions that do this, however I don't know of anything in the OpenSource world that does this, or does anything like this. 

====================
  Explanation.
====================
The problem getting your traditional COBOL application running with a web interface, is that the hyper text transfer protocol (HTTP) is not designed to run an application using the traditional COBOL I/O methods. HTTP is designed to connect and run single I/O requests/response from --> to the client browser, and then disconnect. This has always been inefficient, and still is horribly inefficient for session based applications.  The traditional COBOL applications want to continue running using an infinite number of I/O's (ACCEPT/DISPLAY) requests. 

Slangio is basically a web-socket application server written in "C", plus a web-based client “single page app” terminal emulator that includes all the HTML5 DOM elements. Similar to "single page app" developed using Bootsctrap & AngularJS, except no HTTP protocol or AJAX. Instead it is %100 web-socket protocol allowing a COBOL application to stay alive doing multiple I/O's (DISPAY/ACCEPT) to a web browser just like a terminal. 

(Not just COBOL) This would also work with any language that can support standard I/O (0,1,2) stdin, stdout, stderr. Although SlangIO runs over a web-socket, piped thru stdio, the format of the payload is totally up-to an agreement between the server application and the browser client. In my COBOL applications on the server I would use JSON Objects that contain both instructions for the browser to create/destroy/modify DOM Elements, and the data values for the DOM elements. Similarly the client would also send a JSON Objects containing Instructions, Events, and Data back to the server application. The structure of these Objects can be formatted anyway I want, therefore the name SlangIO, you make it up as you go. You could send exactly what the old terminal was expecting, as long you code the Javascript on the client to know what to do with it.

====================
  Source files.
====================
---------------
application.cob
---------------
 * Simple COBOL example working with a SlangIO server.
 * Author: Michael Anderson
 * License: GPLv3

---------------
appmain.c
---------------
 * Simple C example working with a SlangIO server.
 * Author: Michael Anderson
 * License: GPLv3
 
---------------
base64_enc.c
---------------
 * base64 encoder (RFC3548)
 * Author: Daniel Otte
 * License: GPLv3
 
---------------
base64_enc.h
---------------
 * base64 encoder (Header)
 * Author: Daniel Otte
 * License: GPLv3

---------------
browser.c
---------------
 * Browser working with a SlangIO server.
 * Author: Michael Anderson
 * License: GPLv3

This is a not a real browser, it was used to test the server before the web-socket logic was in place. Now it is not needed or usable. However, upon second thought, it will be used to test non-web clients, as these should also be supported.

---------------
libslangio.c
---------------
 * SlangIO function library.
 * Author: Michael Anderson
 * License: GPLv3

---------------
security.cob
---------------
 * Simple COBOL example working with a SlangIO server.
 * Author: Michael Anderson
 * License: GPLv3

---------------
sha1.c
---------------
 * Author	Daniel Otte
 * Date	2006-10-08
 * License GPLv3 or later
 * Brief SHA-1 implementation.

---------------
sha1.h
---------------
 * Author	Daniel Otte
 * Date	2006-10-08
 * License GPLv3 or later
 * Header.

---------------
slangio.h
---------------
 * Author	Michael Anderson
 * Date	2006-10-08
 * License GPLv3 or later
 * Header.

---------------
slangio_main.c
---------------
 * Author	Michael Anderson
 * Date	Feb. 2015
 * License GPLv3 or later
 * This is the main process in the SlangIO session.

This program is forked and daemonized by the SlangIO Server process.

---------------
slangio_server.c
---------------
 * Author	Michael Anderson
 * Date	Feb. 2015
 * License GPLv3 or later
 * This is the SlangIO Server process.

---------------
websocket.c
---------------
 * Author	Putilov Andrey
 * Date	2013
 * License GPLv3 or later
 * Web socket hand-shake and payload processing.

---------------
websocket.h
---------------
 * Author	Putilov Andrey
 * Date	2013
 * License GPLv3 or later
 * header .

