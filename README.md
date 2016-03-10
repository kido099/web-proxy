# web-proxy
I implemented a Web proxy that lies between your browser and the Internet.

All HTTP and HTTPS requests sent from your browser to any Web site’s origin server should go through your proxy, which will then forward any responses from origin servers back to the browser. If you implement your proxy correctly, then after changing your browser settings to indicate that you would like to use your proxy as a Web proxy, you should be able to browse the Web and view Web sites just as you could when you were not using your proxy.

The basic work flow of your proxy should be as follows:
1. From the command line (most likely on a Linux computer), start your proxy so that it listens on a certain port given as a command-line argument. Port 8080 would be a reasonable choice.
2. In your browser,when you type a URL such as http://www.cnn.com, the request should be forwarded to your proxy. This can be done by going into your browser settings and indicating that you want to use a Web proxy. You need to specify the host and the port of the proxy. Then your browser will then send all HTTP and HTTPS requests to your proxy.
3. Upon receiving a request for a new connection, the proxy should spawn a thread to process it. By reading from the connection, the thread learns whether the request is GET for HTTP or CONNECT for HTTPS.
4. In the case of GET, the request includes a URL that contains both a host/port and a path. The proxy uses the URL to fetch the content from the origin web server, and as it receives the content, it passes it on to the proxy.
5. In the case of CONNECT, the request includes a host/port, but no path. The proxy should open a connection to the origin server, and send an HTTP 200 status code to the browser. It should then begin to forward any data from the origin server to the browser and from the browser to the origin server without any further processing or examina- tion of the data. Because you need to transfer data in both directions simultaneously, you should spawn a second thread to help process the request.
6. When there is no more data to be transfered, close the connection to the web server and the connection to the browser.