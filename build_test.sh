cobc -g -x browser.c libslangio.c -lev
cobc -g -x -lpthread slangio_server.c libslangio.c
cobc -g -x appmain.c libslangio.c -lev
cobc -x slangio_main.c libslangio.c websocket.c sha1.c base64_enc.c -lev
cp -p ./appmain /volume1/applications/
cp -p ./browser /volume1/applications/
cp -p ./slangio_main /volume1/applications/
cp -p ./slangio_server /volume1/applications/

