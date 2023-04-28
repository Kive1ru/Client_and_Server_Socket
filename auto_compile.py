import os

# Please use the two commands below to compile your code.
# Feel free to modify the commands and use any switches you like as long as it compiles on openlab.
# Please do not change the names of the executables.
os.system('gcc "Client Domain/client.c" -o "Client Domain/client" -lssl --lcrypto')
os.system('gcc "Server Domain/server.c" -o "Server Domain/server" -lssl --lcrypto -pthread')

# If you want to compile and test your code in a single python script, uncomment the below two commands.
# Note: please make sure to re-comment or delete the two commands below when you submit your auto_compile.py
# os.system("./Client\ Domain/client user_commands.txt 127.0.0.1")
# os.system("./Server\ Domain/server 127.0.0.1")




