#include "main.h"

//GLOBAL VARIABLE
bool exitProgram;
bool logged = FALSE;
int client;
int serverIPC;
char userSender[30];
message_buf buffer;

//A function that complements the message with the corresponding data for the server
void setBuffer(message_buf *buffer, long type, char userRecipient[30], bool signal, char message[200], int msgPriority){
	buffer->type = type;
	strncpy(buffer->userSender, userSender, 30);
	strncpy(buffer->userRecipient, userRecipient, 30);
	buffer->signal = signal;
	strncpy(buffer->message, message, 200);
	buffer->msgPriority = msgPriority;
}

//The module responds for communication with the server and the user of the program
void commandShell(void){	
	char command[20];
	char userRecipient[30];
	char message[200];
	int msgPriority = 0;
	
	if(serverIPC == -1){
		printf("Cannot connect to server!!!\n");
		serverIPC = msgget(3139, 0);
	}
	
	printf("->");
	fgets(command, 20, stdin);
	command[strlen(command)-1] = '\0';
	
	if(strcmp(command, "help")==0){
		printf("Commands:\n");
		printf("-> c - check msg.\n");
		printf("-> login - login to account.\n");
		printf("-> logout - logout form account.\n");
		printf("-> message - send message to client\n");
		printf("-> messageg - send message to group.\n");
		printf("-> showall - see all clients.\n");
		printf("-> showactive - see all active /login clients.\n");
		printf("-> showgroups - see all groups.\n");
		printf("-> showallg - see all clients from specify group.\n");
		printf("-> join - join to group.\n");
		printf("-> leave - leave group.\n");
		printf("-> blacklist - add client to blacklist.\n");
		printf("-> blacklistg - add group to blacklist.\n");
		printf("-> exit - exit (and logout) from program.\n");
		printf("-> help - print help.\n");
	}
	else if(strcmp(command, "c")==0){
		printf("\n");
	}
	/*
		When the client logs on to the server:
			1. Checks whether the IPC queue is created on the server.
			2. If not, the client is not logged on.
			3. If so, then it is creating a temporary IPC client queue through which it will follow
			temporary data exchange.
			4. After successfully logging in and receiving the key to the individual queue, the client destroys the temporary IPC queue.
	*/
	else if(strcmp(command, "login")==0){
		if(logged == TRUE){
			printf("You are already logged in.\n");
		}
		else{
			printf("Provide a customer name: ");
			fgets(userSender, 30, stdin);
			userSender[strlen(userSender)-1] = '\0';
			
			printf("Password: ");
			fgets(message, 30, stdin);
			message[strlen(message)-1] = '\0';
			
			if(serverIPC != -1){
				printf("Wait for authorisation.\n");
				int tempQueue = msgget(IPC_PRIVATE, 0644|IPC_CREAT);
				
				setBuffer(&buffer, LOGIN, "", tempQueue, message, 1);
				msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
				while(msgrcv(tempQueue, &buffer, sizeof(message_buf)-sizeof(long), 0, 0) == -1);
				
				if(buffer.signal != 0){
					printf("Authorization granted.\n");
					logged = TRUE;
					client = buffer.signal;
				}
				else{
					printf("Access isn't granted!!!\n");
					printf("%s\n",buffer.message);
					strcpy(userSender, "no_logged");
				}
				msgctl(tempQueue, IPC_RMID, 0);
			}
		}
	}
	else if(strcmp(command, "logout")==0){
		if(logged == FALSE){
			printf("You are not logged in!\n");
		}
		else{
			setBuffer(&buffer, LOGOUT, "", 0, "", 1);
			msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
			logged = FALSE;
			printf("You are logged out.\n");
		}
	}
	else if(strcmp(command, "exit")==0){
		if(logged == TRUE){
			setBuffer(&buffer, LOGOUT, "", 0, "", 1);
			msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
			logged = FALSE;
			exitProgram = TRUE;
		}
		else{
			exitProgram = TRUE;
		}
	}
	/*
		Sending a message to a server (message, message): 
		1. Specifying a recipient. 
		2. Enter the message. 
		3. Waiting for the message to be relayed or rejected by the server.
	*/
	else if(strcmp(command, "message")==0){
		if(logged == FALSE){
			printf("You are not logged in!\n");
		}
		else{
			printf("Enter the recipient's name: ");
			fgets(userRecipient, 30, stdin);
			userRecipient[strlen(userRecipient)-1] = '\0';
			
			printf("Enter message (limit to 200 characters):\n");
			fgets(message, 200, stdin);
			
			while(!((msgPriority > 0) && (msgPriority < 11))){
				printf("Message priority (1 to 10): ");
				scanf("%d", &msgPriority);
				getchar();
			}
			
			setBuffer(&buffer, MSG, userRecipient, FALSE, message, msgPriority);
			msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
			msgrcv(client, &buffer, sizeof(message_buf)-sizeof(long), RECMSG, 0);
			
			if(buffer.signal != 0){
				printf("Message sent.\n");
			}
			else{
				printf("You can not send a message!!!\n");
			}
		}
	}
	else if(strcmp(command, "messageg")==0){
		if(logged == FALSE){
			printf("You are not logged in!\n");
		}
		else{
			printf("Enter the recipient's name: ");
			fgets(userRecipient, 30, stdin);
			userRecipient[strlen(userRecipient)-1] = '\0';
			
			printf("Enter message (limit to 200 characters):\n");
			fgets(message, 200, stdin);
			
			while(!((msgPriority > 0) && (msgPriority < 11))){
				printf("Message priority (1 to 10): ");
				scanf("%d", &msgPriority);
				getchar();
			}
			
			setBuffer(&buffer, MSGG, userRecipient, FALSE, message, msgPriority);
			msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
			msgrcv(client, &buffer, sizeof(message_buf)-sizeof(long), RECMSG, 0);
			
			if(buffer.signal != 0){
				printf("Message sent.\n");
			}
			else{
				printf("You can not send a message!!!\n");
			}
		}
	}
	/*When the display command is executed:
		- all customers (showall)
		- active customers (showactive)
		- groups (showgroups)
		- members of a given group (showallg)
		then the client does not have to be logged in (as a no_logged client), then (even if it is logged in)
		An additional IPC message queue is created which is destroyed after retrieving all the information
		from the server.
	*/
	else if(strcmp(command, "showall")==0){
		if(serverIPC != -1){
			int tempIPC = msgget(IPC_PRIVATE, 0644|IPC_CREAT);
			
			setBuffer(&buffer, SHOWALL, "", tempIPC, "", 1);
			msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
			buffer.signal = FALSE;
		
			printf("All users:\n");
			do{
				msgrcv(tempIPC, &buffer, sizeof(message_buf)-sizeof(long), RMSG, 0);
				printf("%s\n",buffer.message);
			}while(buffer.signal != TRUE);
			msgctl(tempIPC, IPC_RMID, 0);
		}
	}
	else if(strcmp(command, "showactive")==0){
		if(serverIPC != -1){
			int tempIPC = msgget(IPC_PRIVATE, 0644|IPC_CREAT);
			
			setBuffer(&buffer, SHOWACTIVE, "", tempIPC, "", 1);
			msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
			buffer.signal = FALSE;
		
			printf("All active users:\n");
			do{
				msgrcv(tempIPC, &buffer, sizeof(message_buf)-sizeof(long), RMSG, 0);
				printf("%s\n",buffer.message);
			}while(buffer.signal != TRUE);
			msgctl(tempIPC, IPC_RMID, 0);
			}
	}
	else if(strcmp(command, "showgroups")==0){
		if(serverIPC != -1){
			int tempIPC = msgget(IPC_PRIVATE, 0644|IPC_CREAT);
			
			setBuffer(&buffer, SHOWGROUPS, "", tempIPC, "", 1);
			msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
			buffer.signal = FALSE;
		
			printf("All available groups:\n");
			do{
				msgrcv(tempIPC, &buffer, sizeof(message_buf)-sizeof(long), RMSG, 0);
				printf("%s\n",buffer.message);
			}while(buffer.signal != TRUE);
			msgctl(tempIPC, IPC_RMID, 0);
		}
	}
	else if(strcmp(command, "showallg")==0){
		if(serverIPC != -1){
			int tempIPC = msgget(IPC_PRIVATE, 0644|IPC_CREAT);
			
			printf("Give group's name:");
			fgets(userRecipient, 30, stdin);
			userRecipient[strlen(userRecipient)-1] = '\0';
		
			setBuffer(&buffer, SHOWMEMBERS, userRecipient, tempIPC, "", 1);
			msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
			buffer.signal = FALSE;
		
			printf("All members of the group %s:\n", userRecipient);
			do{
				msgrcv(tempIPC, &buffer, sizeof(message_buf)-sizeof(long), RMSG, 0);
				printf("%s\n",buffer.message);
			}while(buffer.signal != TRUE);
			msgctl(tempIPC, IPC_RMID, 0);
		}
	}
	//******************************************************************************************
	/*
		Adding a customer to a group:
		1. Specify the group to which you want to add the client.
		2. Waiting for confirmation from the server.
	*/
	else if(strcmp(command, "join")==0){
		if(logged == FALSE){
			printf("You are not logged in!\n");
		}
		else{
			printf("Give group's name: ");
			fgets(userRecipient, 30, stdin);
			userRecipient[strlen(userRecipient)-1] = '\0';
			
			setBuffer(&buffer, JOIN, userRecipient, 0, "", 1);
			msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
			msgrcv(client, &buffer, sizeof(message_buf)-sizeof(long), RECMSG, 0);
			
			if(buffer.signal != 0){
				printf("You joined the group: %s.\n", userRecipient);
			}
			else{
				printf("You can not add you to a group: %s!!!\n", userRecipient);
			}
		}
	}
	/*
		Leaving the group:
		1. Specify the group from which you want to remove the client.
		2. Waiting for confirmation from the server.
	*/
	else if(strcmp(command, "leave")==0){
		if(logged == FALSE){
			printf("You are not logged in!\n");
		}
		else{
			printf("Give group's name: ");
			fgets(userRecipient, 30, stdin);
			userRecipient[strlen(userRecipient)-1] = '\0';
			
			setBuffer(&buffer, LEAVE, userRecipient, 0, "", 1);
			msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
			msgrcv(client, &buffer, sizeof(message_buf)-sizeof(long), RECMSG, 0);
			
			if(buffer.signal != 0){
				printf("You left the group: %s.\n", userRecipient);
			}
			else{
				printf("Error!!! You can not leave: %s!!!\n", userRecipient);
				printf("Causes: \n");
				printf("\t(a)You are not in the group.\n");
				printf("\t(b) Unforeseen error.\n");
			}
		}
	}
	/*
		Adding a client to a blacklist:
		1. Specify the client that is to be blocked.
		2. Waiting for confirmation from the server.
	*/
	else if(strcmp(command, "blacklist")==0){
		if(logged == FALSE){
			printf("You are not logged in!\n");
		}
		else{
			printf("Enter user name to block: ");
			fgets(userRecipient, 30, stdin);
			userRecipient[strlen(userRecipient)-1] = '\0';
			
			setBuffer(&buffer, BLACKLIST, userRecipient, 0, "", 1);
			msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
			msgrcv(client, &buffer, sizeof(message_buf)-sizeof(long), RECMSG, 0);
			
			if(buffer.signal != 0){
				printf("User: %s is blocked.\n", userRecipient);
			}
			else{
				printf("You can not block: %s!!!\n", userRecipient);
			}
		}
	}
	/*
		Adding a group to the black list: 
		1. Specify the group to block. 
		2. Waiting for confirmation from the server.
	*/
	else if(strcmp(command, "blacklistg")==0){
		if(logged == FALSE){
			printf("You are not logged in!\n");
		}
		else{
			printf("Enter group name to block: ");
			fgets(userRecipient, 30, stdin);
			userRecipient[strlen(userRecipient)-1] = '\0';
			
			setBuffer(&buffer, BLACKLISTG, userRecipient, 0, "", 1);
			msgsnd(serverIPC, &buffer, sizeof(message_buf)-sizeof(long), 0);
			msgrcv(client, &buffer, sizeof(message_buf)-sizeof(long), RECMSG, 0);
			
			if(buffer.signal != 0){
				printf("You blocked the group: %s.\n", userRecipient);
			}
			else{
				printf("You can not block groups: %s!!!\n", userRecipient);
			}
		}
	}
	else if(strcmp(command, "\0")==0){
		printf("\n");
	}
	else{
		printf("Undefine command!!! Need help? Write: help ;)\n");
	}
}

// Function responsible for receiving messages from the server and their output in the console
// If there is no data in the IPC queue, the process is not paused
// The message type is not used, but the message is printed when the com- mand type is RMSG
void checkMessageBox(void){	
	if((serverIPC != -1) && (client != -1)){
		while(msgrcv(client, &buffer, sizeof(message_buf)-sizeof(long), 0, IPC_NOWAIT) != -1){
			if(buffer.type == RMSG){
				char *tempTimeGet;
				struct tm * timeinfo;
				
				timeinfo = localtime (&buffer.timeSend);
				tempTimeGet = asctime (timeinfo);
				tempTimeGet[strlen(tempTimeGet)-1]='\0';
				
				printf("\nMessage from %s:\n", buffer.userSender);
				printf("[%s]:>%s\n", tempTimeGet, buffer.message);
			}
		}
	}
}

int main(){
	
	printf("**********************TeamChat**********************\n");
	printf("To get help, write: help ;)\n");
	strcpy(userSender, "no_logged");
	
	client = -1;
	serverIPC = msgget(3139, 0);
	exitProgram = FALSE;
	
	while(exitProgram != TRUE){
		commandShell();
		checkMessageBox();
	}
	
	return 0;
}
