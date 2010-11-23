######################################################################################
# ActionScript socket server builder
######################################################################################

add_executable(as3-server server/as3-server.c)
target_link_libraries(as3-server freenect m)
