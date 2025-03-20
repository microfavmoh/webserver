# webserver
A work in progress webserver wirtten in c which has two text files required for it functioning
## banned.txt
in this file there will be the names of the files that won't be loaded by the webserver and thus won't be sent to the client if requested
## header.txt
which contains the http header which is currently not customizable and can be found in the repositiory and the only part that can be currently removed is the date: %s line as it is currently unused
# Note:
the current version of the webserver isn't fit for production code and doesn't use https
