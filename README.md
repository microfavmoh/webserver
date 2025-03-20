# webserver
A work in progress webserver wirtten in c which has two text files required for it functioning
## banned.txt
in this file there will be the names of the files that won't be loaded by the webserver and thus won't be sent to the client if requested and these files relative paths have to be put in this file and it isn't required that the file names/paths are seperated by newlines but for readability it is suggested
## header.txt
which contains the http header which is currently not customizable and can be found in the repositiory and the only part that can be currently removed is the date: %s line as it is currently unused
# Note:
the current version of the webserver is unfit for production code as it doesn't use https among other things
## Building
if you for some reason still want to check out this project go ahead fork the repository and build the project
