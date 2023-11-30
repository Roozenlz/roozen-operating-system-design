cmd_/root/CLionProjects/czxt/mymodule.mod := printf '%s\n'   mymodule.o | awk '!x[$$0]++ { print("/root/CLionProjects/czxt/"$$0) }' > /root/CLionProjects/czxt/mymodule.mod
