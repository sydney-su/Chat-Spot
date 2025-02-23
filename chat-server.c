#include "http-server.h"
#include "./chat-server.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

char const HTTP_404_NOT_FOUND[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n";
char const HTTP_400_INVALID[] = "HTTP/1.1 400 Invalid Response\r\nContent-Type: text/plain\r\n\r\n";
char const HTTP_500_CAPPED[] = "HTTP/1.1 500 Max Size Reached\r\nContent-Type: text/plain\r\n\r\n";
char const HTTP_200_OK[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n";
uint32_t const MAX_POSTS = 100000;
uint8_t const MAX_REACTIONS = 100;
uint8_t const MAX_USER_LEN = 15;
uint8_t const MAX_POST_LEN = 255;
uint8_t const MAX_REACTION_LEN = 15;
uint8_t const MAX_BUFF = 100;

struct ChatChats {
	uint32_t size;
	uint32_t capacity;
	Chat** chats;
};

typedef struct ChatChats ChatChats;
ChatChats chat_chats;

void initialize_chat() {
	chat_chats.size = 0;
	chat_chats.capacity = 2;
 	chat_chats.chats = calloc(2, sizeof(Chat*));	
}

void expand_chats() {
	uint32_t new_capacity = chat_chats.capacity * 2;
	Chat** new_chats = realloc(chat_chats.chats, new_capacity * sizeof(Chat*));
	chat_chats.chats = new_chats;
	chat_chats.capacity = new_capacity;
}

void expand_chat_reactions(Chat *c) {
	int32_t new_capacity = c->capacity_reactions * 2;
	Reaction** new_reactions = realloc(c->reactions, new_capacity * sizeof(Reaction*));
	c->reactions = new_reactions;
	c->capacity_reactions = new_capacity;
}

uint8_t add_chat(char* username, char* message) {
	if(chat_chats.size == chat_chats.capacity)
		expand_chats();
	Chat *new_chat = malloc(sizeof(Chat));
	new_chat->id = chat_chats.size + 1;
	new_chat->user = username;
	new_chat->message = message;
	char buffer[MAX_BUFF];
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);	
	char* time = malloc(sizeof(buffer));

	strncpy(time, buffer, sizeof(buffer));
	new_chat->timestamp = time;
	new_chat->num_reactions = 0;
	new_chat->capacity_reactions = 4;
	new_chat->reactions = calloc(4, sizeof(Reaction*));
	chat_chats.chats[chat_chats.size] = new_chat;
	chat_chats.size += 1;
	return 1;
}
uint8_t add_reaction(char* username, char* message, char* id) {		
	Reaction* new_reaction = malloc(sizeof(Reaction));
	new_reaction->user = username;
	new_reaction->message = message;

	for(int i = 0; i < chat_chats.size; i++) {
		Chat* c = chat_chats.chats[i];
		if (c->id == atoi(id)){
			if(c->num_reactions == MAX_REACTIONS) {
				free(username);
				free(message);
				free(id);
				free(new_reaction);
				return 2;
			}
			if(c->num_reactions + 1 == c->capacity_reactions)
				expand_chat_reactions(c);
			c->reactions[c->num_reactions] = new_reaction;
			c->num_reactions += 1;	
			free(id);
			return 1;
		}
	}
	free(username);
	free(message);
	free(id);
	free(new_reaction);
	return 0;	
}

void reset(int client_sock) {
	char response_buff[BUFFER_SIZE];
	snprintf(response_buff, BUFFER_SIZE, "");
	write(client_sock, HTTP_200_OK, strlen(HTTP_200_OK));  	
	write(client_sock, response_buff, strlen(response_buff));

	for(int i = 0; i < chat_chats.size; i++) {
		Chat* c = chat_chats.chats[i];
		free(c->timestamp);
		free(c->user);
		free(c->message);
		for(int j = 0; j < c->num_reactions; j++) {
			Reaction *r = c->reactions[j];
			free(r->user);
			free(r->message);
			free(r);
		}
		free(c->reactions);
		free(c);
	}
	free(chat_chats.chats);
	chat_chats.chats = NULL;
	chat_chats.size = 0;
	chat_chats.capacity = 0;
}

void respond_with_chats(int client_sock) {
	write(client_sock, HTTP_200_OK, strlen(HTTP_200_OK));  
	Chat* c;
	Reaction* r;	
	for(int i = 0; i < chat_chats.size; i++) {
		c = chat_chats.chats[i];
		char buff[BUFFER_SIZE];
		snprintf(buff, BUFFER_SIZE, "[#%d %s] %12s: %s\n", c->id, c->timestamp, c->user, c->message);
		write(client_sock, buff, strlen(buff));
		if(c->num_reactions > 0) {
			for(int j = 0; j < c->num_reactions; j++) {
				r = c->reactions[j];
				char buff2[BUFFER_SIZE];
				snprintf(buff2, BUFFER_SIZE, "%30s(%s)  %s\n", "", r->user, r->message);
				write(client_sock, buff2, strlen(buff2));
			}
		}
	}
	char response_buff[BUFFER_SIZE];
	snprintf(response_buff, BUFFER_SIZE, "\n");
	write(client_sock, response_buff, strlen(response_buff));
}

uint8_t handle_post(char* path, int client_sock) {
	if(chat_chats.size == MAX_POSTS) {
		write(client_sock, HTTP_500_CAPPED, strlen(HTTP_500_CAPPED));  
		printf("SERVER LOG: can't post, maximum posts (%d) reached. Path \"%s\"\n", MAX_POSTS, path);	
		char response_buff[BUFFER_SIZE];
		snprintf(response_buff, BUFFER_SIZE, "Error 500: can't post, maximum posts (%d) reached\n", MAX_POSTS);
		write(client_sock, response_buff, strlen(response_buff));
		return 0;
	}

	char response_buff[BUFFER_SIZE];
	char *p1, *p2;
	p1 = strstr(path, "/post?user=");
	p2 = strstr(path, "&message=");
	uint16_t user_len = 0;
	uint32_t msg_len = 0;
	if((p1 != 0) & (p2 != 0)) {
		p1 += 11;
		user_len = p2 - p1;
		p2 += 9;
		msg_len = path + strlen(path) - p2;
	}
	if((p1 == 0) | (p2 == 0) | (user_len == 0) | (msg_len == 0) | (user_len > MAX_USER_LEN) | (msg_len > MAX_POST_LEN)) {
		write(client_sock, HTTP_400_INVALID, strlen(HTTP_400_INVALID));  
		printf("SERVER LOG: missing or invalid parameters for post from path \"%s\"\n", path);	
		char response_buff[BUFFER_SIZE];
		snprintf(response_buff, BUFFER_SIZE, "Error 400: missing or invalid parameters for post\n");
		write(client_sock, response_buff, strlen(response_buff));
		return 0;
	}
	
	char *user, *msg;
	user = malloc(user_len + 1);
	msg = malloc(msg_len + 1);
	user[user_len] = '\0';
	msg[msg_len] = '\0';
	strncpy(user, p1, user_len);
	strncpy(msg, p2, msg_len);
	add_chat(user, msg);
	respond_with_chats(client_sock);

	return 1;
}

uint8_t handle_reaction(char*path, int client_sock) {
	char response_buff[BUFFER_SIZE];
	char *p1, *p2, *p3;
	p1 = strstr(path, "/react?user=");
	p2 = strstr(path, "&message=");
	p3 = strstr(path, "&id=");
	uint16_t user_len = 0;
	uint32_t msg_len = 0;
	uint16_t id_len = 0;

	if((p1 != 0) & (p2 != 0) & (p3 != 0)) {
		p1 += 12;
		user_len = p2 - p1;
		p2 += 9;
		msg_len = p3 - p2;
		p3+= 4;
		id_len = path + strlen(path) - p3;
	}

	if((p1 == 0) | (p2 == 0) | (p3 == 0) | (user_len == 0) | (msg_len == 0) | (id_len == 0) |
			(user_len > MAX_USER_LEN) | (msg_len > MAX_REACTION_LEN)) {
		write(client_sock, HTTP_400_INVALID, strlen(HTTP_400_INVALID));  
		printf("SERVER LOG: missing or invalid parameters for reaction. Path \"%s\"\n", path);	
		char response_buff[BUFFER_SIZE];
		snprintf(response_buff, BUFFER_SIZE, "Error 400: missing or invalid parameters for reaction\n");
		write(client_sock, response_buff, strlen(response_buff));
		return 0;
	}

	char *user, *msg, *id;
	user = malloc(user_len + 1);
	msg = malloc(msg_len + 1);
	id = malloc(id_len + 1);
	user[user_len] = '\0';
	msg[msg_len] = '\0';
	id[id_len] = '\0';
	strncpy(user, p1, user_len);
	strncpy(msg, p2, msg_len);
	strncpy(id, p3, id_len);

	int n = add_reaction(user, msg, id);
	if (n == 0) {
		write(client_sock, HTTP_400_INVALID, strlen(HTTP_400_INVALID));  
		printf("SERVER LOG: HTTP Code 400 - no matching chat id from path \"%s\"\n", path);
		char response_buff[BUFFER_SIZE];
		snprintf(response_buff, BUFFER_SIZE, "Error 400: no matching ID in existing chats\n");
		write(client_sock, response_buff, strlen(response_buff));
	}
	else if (n == 2) {
		write(client_sock, HTTP_500_CAPPED, strlen(HTTP_500_CAPPED));  
		printf("SERVER LOG: can't post, maximum reactions (%d) reached for specified post id from path \"%s\"\n", MAX_REACTIONS, path);	
		char response_buff[BUFFER_SIZE];
		snprintf(response_buff, BUFFER_SIZE, "Error 500: can't react, maximum reactions (%d) reached for specified post id\n", MAX_REACTIONS);
		write(client_sock, response_buff, strlen(response_buff));
	}
	else if (n == 1)
		respond_with_chats(client_sock);

	return 1;
}

int hex_to_dec(char* hex) {
	int value;
	sscanf(hex, "%2x", &value);
	return value;
}

char* parsedURL(char* path) {
	int len = strlen(path); 		
	char* new_path = malloc(len + 1);
	char* p = new_path;
	for(int i = 0; i < len; i++) {
		if((path[i] == '%') & (i + 2 < len)) {
			if(isxdigit(path[i + 1]) & isxdigit(path[i + 2])) {
				*p++ = hex_to_dec(path + i + 1);
				i += 2;
			}
			else
				*p++ = path[i];
		}
		else
			*p++ = path[i];
	}
	*p = '\0';
	return new_path;
}

void handle_404(int client_sock, char *path)  {
    printf("SERVER LOG: Got request for unrecognized path \"%s\"\n", path);
    char response_buff[BUFFER_SIZE];
    snprintf(response_buff, BUFFER_SIZE, "Error 404: unrecognized path \"%s\"\n", path);
    write(client_sock, HTTP_404_NOT_FOUND, strlen(HTTP_404_NOT_FOUND));
    write(client_sock, response_buff, strlen(response_buff));
}

void handle_response(char* request, int client_sock) {
	printf("\nSERVER LOG: Got request: \"%s\"\n", request);
		
	if((chat_chats.chats == NULL))
		initialize_chat();

	char *start, *end, *temp_path;
	start = strstr(request, "GET ");
	end = strstr(request, " HTTP/1.1");
	uint32_t path_len = 0;
	if((start != 0) & (end != 0)) {
		start += 4;
		path_len = end - start;
	}
	else {
		printf("Invalid request line\n");
		return; 
	}

	temp_path = malloc(path_len + 1);
	strncpy(temp_path, start, path_len);
	temp_path[path_len] = '\0';
	char* path = parsedURL(temp_path);
	
	if(strcmp(path, "/") == 0) {
		write(client_sock, HTTP_200_OK, strlen(HTTP_200_OK));  
		char response_buff[BUFFER_SIZE];
		snprintf(response_buff, BUFFER_SIZE, "HOME\nwelcome to the chat server :)\nyou are in the root directory!\n");
		write(client_sock, response_buff, strlen(response_buff));
	}
	else if(strcmp(path, "/chats") == 0)
		respond_with_chats(client_sock);	
    	else if(strstr(path, "/post?user=")) 
		handle_post(path, client_sock);
   	else if(strstr(path, "/react?user=")) 
		handle_reaction(path, client_sock);
    	else if(strstr(path, "/reset")) {
		reset(client_sock);
	}
	else
		handle_404(client_sock, path);
	free(path);	
	free(temp_path);
}

int main(int argc, char *argv[]) {
	int port = 0;
	if(argc >= 2)
		port = atoi(argv[1]);
	start_server(&handle_response, port);
}
