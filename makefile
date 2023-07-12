server_obj := server_f.cpp server.cpp
client_obj := client_f.cpp client.cpp


server: $(server_obj)
	g++ $(server_obj) -o server -pthread

run_server:
		./server

client: $(client_obj)
	g++ $(client_obj) -o client -pthread


