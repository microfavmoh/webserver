# webserver
A work in progress webserver wirtten in c (windows only)
if you want to checkout this project follow the following steps
## 1 - Cloning
to clone the repository open a terminal and run the command
```
git clone https://github.com/microfavmoh/webserver.git
```
## 2 - Building
open a terminal in the build directory and run the command
```
cmake ..
```
this will produce the solution file on windows and the makefile on linux 
## How to use
in the assets folder in the repository there are some example pages to show functionatlity.
If you want to host a page a let's say dir/example/ then you'd need to create two folders inside 
one another dir and inside of it create example and inside of that put index.html.
Now to actually visit the pages visit your-ip-address/path-to-page so if your ip address is 192.0.0.1
and you want to visit the aforementioned page you can visit 192.0.0.1/dir/example/. to shutdown the server enter q or any string containing that letter into the terminal
## Required files
### 1 - header.txt
the http header currently not customizable and that can be found in the repository and is required
for the server's functioning
### 2 - assets/404.html
in the assets folder the 404 page is required and if not included will cause the program to crash when a request
from a non-existent page is received