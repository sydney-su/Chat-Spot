#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

struct Reaction {
	char* user;
	char* message;
};

typedef struct Reaction Reaction;

struct Chat {
	uint32_t id;
	char* user;
	char* message;
	char* timestamp;
	uint32_t num_reactions;
	uint32_t capacity_reactions;
	Reaction** reactions;
};

typedef struct Chat Chat;

uint8_t add_chat(char* username, char* message);
uint8_t add_reaction(char* username, char* message, char* id);
void reset();
void respond_with_chats(int client);
uint8_t handle_post(char* path, int client);
uint8_t handle_reaction(char*path, int client);

#endif
