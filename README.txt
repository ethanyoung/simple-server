File in this distribution:
	README.txt		for imformation
	fork-web-server.c	source file
	makefile		makefile

1. How to compile and run the program.
The complier is gcc and the dialect is gnu89. To compile the source code and build the program, just use "make" command in the source code's directory. Then the program named "forking-web-server" will be created in the directoty.

To run: Type "./forking-web-server [filename] [port number]" in the directory to run the program. E.g. "./forking-web-server hello_world.html 8000" will run the program to serve on the port of 8000, with serving file of "hello_world.html". Before running the program, make sure that the extension of the file is listed in "mime-types.tsv".

2. Functions supported
The program works as a server that can serve multiple clients simultaneously. It provides functions below:
 * Serve file to the web browser(supporting HTTP).
 * Handle multiple connections at the same time.
 * Serve file with the filename specified as the first command line argument.
 * Serve on the port with the port number specified as the second command line argument.
 * Check whether the extension of the file listed in "mime-types.tsv". 
 * The number of the types supported is the same as the number of types listed in "mime-types.tsv".

3. Functions not supported
 * Do not set the limitation for maximum connections.
 * When browser downloads some particular kinds of file(media files), the file is not named appropriately.

4. Known limitations
 * No limitations as of the date when this document finished.


