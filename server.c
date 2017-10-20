#include "main.h"

/*
	Structure containing information about individual customer:
		- ID
		- name
		- password
		- is logged in
		- is blocked
		- the identity of the individual IPC queue
		- number of blocked clients and groups, and list of blocked clients, groups
*/
struct ClientInfo{
	int idClient;
	char name[30];
	char password[30];
	bool logged;
	bool blocked;
	int idQueue;
	int falseLoggedNumber;
	int blockedUsersNumber;
	int blockedUsersID[100];
	int blockedGroupsNumber;
	int blockedGroupsID[100];
};

//Structure containing information about a particular group: ID, name, number of clients, list of clients.
struct GroupInfo{
	int idGroup;
	char name[30];
	unsigned int clientNumber;
	unsigned int clientIn[100];
};

//GLOBAL
struct ClientInfo users[100];
struct GroupInfo groups[100];
int *exitProgram;

//A function that complements the message with the corresponding data for the client
void setBuffer(message_buf *buffer, long type, char userSender[30], char userRecipient[30], bool signal, char message[200], int msgPriority){
	buffer->type = type;
	strncpy(buffer->userSender, userSender, 30);
	strncpy(buffer->userRecipient, userRecipient, 30);
	buffer->signal = signal;
	strncpy(buffer->message, message, 200);
	if(msgPriority == 0){
		time(&(buffer->timeSend));
	}
	buffer->msgPriority = msgPriority;
}

// A function that reads configuration of accounts and groups from files
// and sets the default values for clients and groups.
int readFromFile(int *clientsNumber, int *groupsNumber){
	FILE *fclients = fopen("users.conf", "r");
	FILE *fgroups = fopen("group.conf", "r");
	int count;
	int i, j, k;
	
	if((fclients == NULL) || (fgroups == NULL)){
		return -1;
	}
	fscanf(fclients,"%d", clientsNumber);
	fscanf(fgroups, "%d", groupsNumber);
	count = 0;
	while(!feof(fclients)){
		if(fscanf(fclients, "%d %s %s", &(users[count].idClient), users[count].name, users[count].password) < 3){
			if(count > *clientsNumber){
				return -1;
			}
		}
		users[count].idQueue = -1;
		users[count].logged = FALSE;
		users[count].blocked = FALSE;
		users[count].blockedGroupsNumber = 0; 
		users[count].blockedUsersNumber = 0;
		for(k = 0; k < 100; k++){
			users[count].blockedUsersID[k] = -1;
			users[count].blockedGroupsID[k] = -1;
		}
		users[count].falseLoggedNumber = 0;
		++count;
	}
	count = 0;
	while(!feof(fgroups)){
		if(fscanf(fgroups, "%s %d", groups[count].name, &(groups[count].clientNumber)) < 2){
			if(count > *groupsNumber){
				return -1;
			}
		}
		groups[count].idGroup = count;
		for(i = 0; i < 100; i++){
			groups[count].clientIn[i] = -1;
		}
		j = 0;
		for(i = 0; i < groups[count].clientNumber; i++){
			if(fscanf(fgroups, "%d", &(groups[count].clientIn[i])) < 1){
				if(j > groups[count].clientNumber){
					return -1;
				}
			}
			++j;
		}
		++count;
	}
	
	fclose(fclients);
	fclose(fgroups);
	return 0;
}

//The function returns the group id
int getUserId(char clientName[], int cliensNumber){
	int i;
	
	for(i = 0; i < cliensNumber; i++){
		if(strcmp(clientName, users[i].name)==0){
			return i;
		}
	}
	
	return -1;
}
//The function returns the client ID
int getGroupId(char groupName[], int groupsNumber){
	int i;
	
	for(i = 0; i < groupsNumber; i++){
		if(strcmp(groupName, groups[i].name)==0){
			return i;
		}
	}
	
	return -1;
}

//Error generating function for client
char* generateErrorMessage(char tab[]){
	char *msgError = malloc((strlen(tab)+1)*sizeof(char));
	int i;
	
	for(i = 0; i < strlen(tab); i++){
		msgError[i] = tab[i];
	}
	return msgError;
}

//Module responsible for communication with the client
bool customerService(void){	
	message_buf buffer;
	int listenIPC = msgget(3139, 0644|IPC_CREAT);
	int clientsNumber, groupsNumber, currentClientID, currentGroupID, reciverID;
	int i, j, k, l;
	int tmpQueue, tempIPCcurrentClient;
	bool temp_send__msg_group_member, clientExist;
	char *messageError;
	int helpArryMsgBuffer[10];
	int tempTesting1;
	
	struct MsgBuffer{						//Message Buffer for individual clients who are logged on.
		char userSender[10][30];
		char message[10][200];
		time_t timeSend[10];
		int msgPriority[10];
		int msgInBuffer;
	};
	
	struct MsgBuffer msgBuffer[9];		//Set default values.
	for(i = 0; i < 9; i++){
		msgBuffer[i].msgInBuffer = 0;
		for(j = 0; j < 10; j++){
			msgBuffer[i].msgPriority[j] = -1;
		}
	}
	
	if(readFromFile(&clientsNumber, &groupsNumber) != 0){
		*exitProgram = TRUE;
		printf("Failed to read data from file!!!!\n");
		printf("Press any key to exit.\n");
		getchar();
	}
	while(*exitProgram != TRUE){
	// Receiving a message without considering the message type 
	// The process is not paused as there is no message
		if(msgrcv(listenIPC, &buffer, sizeof(message_buf)-sizeof(long), 0, IPC_NOWAIT) != -1){
			currentClientID = getUserId(buffer.userSender, clientsNumber);
			currentGroupID = getGroupId(buffer.userRecipient, groupsNumber);
			
			switch(buffer.type){
				/*
					Customer login:
						1. Verify that the customer exists in the database.
						2. Verify that you are already logged in.
						3. If the customer is in the database and not logged in, then the check is sent
							Authentication passwords. If the client does not exist, or is logged in, then
							the login application is rejected.
						4. If the password is correct, the server accepts the temporary key for the temporary IPC queue that it created
							The client sends the IPC queue information to the fixed client communication <-> client.
						5. Remove the temporary IPC queue.
						6. Verify that the client has a waiting buffer for sending the message. if so
							Send them in the correct order with respect to the priority.
				*/
				case LOGIN :
					printf("Client %s\n", buffer.userSender);
					tmpQueue = buffer.signal;
					clientExist = FALSE;
					messageError = NULL;
					if((currentClientID != -1) && (users[currentClientID].logged == FALSE)){
						if(users[currentClientID].falseLoggedNumber < 5){
							if(strncmp(buffer.message, users[currentClientID].password, 30)==0){
								clientExist = TRUE;
								printf("Correct login.\n");
							}
							else{
								printf("Incorrect password!!!\n");
								messageError = generateErrorMessage("Incorrect password!!!");
								users[currentClientID].falseLoggedNumber += 1;
								clientExist = FALSE;
							}
						}
						else{
							printf("Client is blocked!\n");
							messageError = generateErrorMessage("Client is blocked!");
							clientExist = FALSE;
						}
					}
					if(clientExist == FALSE){
						if(currentClientID != -1){
							printf("Invalid login!\n");
						}
						
						if(messageError != NULL){
							setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, messageError, 0);
							free(messageError);
						}
						else{
							setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
						}
					}
					else{
						users[currentClientID].logged = TRUE;
						if(users[currentClientID].idQueue == -1){
							users[currentClientID].idQueue = msgget(IPC_PRIVATE, IPC_CREAT|0644);
						}
						setBuffer(&buffer, RECMSG, buffer.userSender, "", users[currentClientID].idQueue, "", 0);
					}
					msgsnd(tmpQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
					msgctl(tmpQueue, IPC_RMID, 0);
					
					// If the client is logged in and there are messages in the store,
					// will be sent to the appropriate message queue in the correct order
					// depending on priority (10 is the highest, 1 is the smallest).
					if(users[currentClientID].logged == TRUE){
						if(msgBuffer[currentClientID].msgInBuffer != 0){
							for(i = 0; i < 10; i++){
								helpArryMsgBuffer[i] = -1;
							}
							l = 0;
							for(i = 0; i < msgBuffer[currentClientID].msgInBuffer ; i++){
								for(j = 10; j > 0; j--){
									for(k = 0; k < 10; k++){
										if(msgBuffer[currentClientID].msgPriority[k] == j){
											helpArryMsgBuffer[l++] = k;
										}
									}
								}
							}			
							for(i = 0; i < msgBuffer[currentClientID].msgInBuffer ; i++){
								buffer.timeSend = msgBuffer[currentClientID].timeSend[helpArryMsgBuffer[i]];
								setBuffer(&buffer, RMSG, msgBuffer[currentClientID].userSender[helpArryMsgBuffer[i]], "", 
																TRUE, msgBuffer[currentClientID].message[helpArryMsgBuffer[i]], 1);
								msgsnd(users[currentClientID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
							}
							msgBuffer[currentClientID].msgInBuffer = 0;
							for(j = 0; j < 10; j++){
								msgBuffer[currentClientID].msgPriority[j] = -1;
							}
						}
					}
					break;
				//***********************************************************************
				/*
					When the client logs off, the IPC queue is removed (assigned by the server to the client).
					Then the identifier and the client blacklists are removed.
				*/
				case LOGOUT :
					printf("%s is logout.\n", buffer.userSender);
					users[currentClientID].logged = FALSE;
					msgctl(users[currentClientID].idQueue, IPC_RMID, 0);
					users[currentClientID].idQueue = -1;
					users[currentClientID].blockedUsersNumber = 0;
					users[currentClientID].blockedGroupsNumber = 0;
					break;
				//***********************************************************************
				/*
					Sending messages between clients.
					1. Verify that the recipient exists in the database. If not, the sender gets the information that the message was not sent.
					2. If the recipient is in the database, it is checked whether the sender is not on the recipient list.
					3. If the recipient does not have a sender on the black list, then the message is sent.
					4. If the black list is empty, it is checked if the recipient is logged in. Not logged in, it is
						the message goes to the buffer. If the buffer overflows, messages will be sent to the IPC queue. As a customer is logged in
						the message is sent to him.
					5. The sender receives the information whether the message was sent / transferred to the buffer or rejected.
				*/
				case MSG :
					printf("Message: %s to %s\n", buffer.userSender, buffer.userRecipient);
					reciverID = getUserId(buffer.userRecipient, clientsNumber);
					if(reciverID != -1){
						if(users[reciverID].logged == FALSE){
							if(users[reciverID].idQueue == -1){
								users[reciverID].idQueue = msgget(IPC_PRIVATE, IPC_CREAT|0644);
							}
						}
						//Checking if the recipient has no sender on the blacklist.
						if((users[reciverID].blockedUsersNumber != 0) || (users[reciverID].blockedGroupsNumber != 0)){
							clientExist = FALSE;
							for(i = 0; i < users[reciverID].blockedUsersNumber; i++){
								if(users[reciverID].blockedUsersID[i] == currentClientID){
									clientExist = TRUE;
								}
							}
							
							if(clientExist == TRUE){
								setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
								printf("Message send failed. Client is blocked!\n");
							}
							else{
								for(i = 0; i < users[reciverID].blockedGroupsNumber; i++){
									for(j = 0; j < groupsNumber; j++){
										if(groups[j].clientNumber != 0){
											for(k = 0; k < groups[j].clientNumber; k++){
												if(groups[j].clientIn[i] == currentClientID){
													if(users[reciverID].blockedGroupsID[i] == groups[j].idGroup){
														clientExist = TRUE;
														k = groups[j].clientNumber;
														j = groupsNumber;
														i = users[reciverID].blockedGroupsNumber;
													}
												}
											}
										}
									}
								}
								
								if(clientExist == TRUE){
									setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
									printf("Message send failed. Client is blocked!\n");
								}
								else{
									buffer.type = RMSG;
									time(&buffer.timeSend);
									msgsnd(users[reciverID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
									buffer.type = RECMSG;
									buffer.signal = TRUE;
									printf("Message send sucesful.\n");
								}
							}
						}
						else{
							if((users[reciverID].logged == FALSE)){
								// If the recipient is not logged in and the message buffer is not full,
								// then save the message in the buffer
								strcpy(msgBuffer[reciverID].userSender[msgBuffer[reciverID].msgInBuffer], buffer.userSender);
								strcpy(msgBuffer[reciverID].message[msgBuffer[reciverID].msgInBuffer], buffer.message);
								time(&msgBuffer[reciverID].timeSend[msgBuffer[reciverID].msgInBuffer]);
								msgBuffer[reciverID].msgPriority[msgBuffer[reciverID].msgInBuffer] = buffer.msgPriority;
								++msgBuffer[reciverID].msgInBuffer;
								tempTesting1 = msgBuffer[reciverID].msgInBuffer;
								// If the buffer is overflowing then messages in the correct order (relative to priority) are
								// sent to the appropriate IPC queue
								if(msgBuffer[reciverID].msgInBuffer == 7){
									for(i = 0; i < 10; i++){
										helpArryMsgBuffer[i] = -1;
									}
									l = 0;
									for(i = 0; i < msgBuffer[reciverID].msgInBuffer ; i++){
										for(j = 10; j > 0; j--){
											for(k = 0; k < 10; k++){
												if(msgBuffer[reciverID].msgPriority[k] == j){
													helpArryMsgBuffer[l++] = k;
												}
											}
										}
									}
									l = 0;				
									for(i = 0; i < msgBuffer[reciverID].msgInBuffer; i++){
										buffer.timeSend = msgBuffer[reciverID].timeSend[helpArryMsgBuffer[l]];
										setBuffer(&buffer, RMSG, msgBuffer[reciverID].userSender[helpArryMsgBuffer[l]], "", 
															TRUE, msgBuffer[reciverID].message[helpArryMsgBuffer[l]], 1);
										msgsnd(users[reciverID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
										++l;
									}
									msgBuffer[reciverID].msgInBuffer = 0;
									for(j = 0; j < 10; j++){
										msgBuffer[reciverID].msgPriority[j] = -1;
									}
								}
									setBuffer(&buffer, RECMSG, buffer.userSender, "", TRUE, "", 0);
									printf("%d-Message send sucesful.\n", tempTesting1);
							}
							else{
									buffer.type = RMSG;
									time(&buffer.timeSend);
									msgsnd(users[reciverID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
									buffer.type = RECMSG;
									buffer.signal = TRUE;
									printf("Message send sucesful.\n");
								}
						}
					}
					else{
						setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
						printf("Message send failed.\n");
					}
					msgsnd(users[currentClientID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
					break;
				//***********************************************************************
				/*
					Sending messages to the whole group.
					1. Verify that the recipient exists in the database. If not, the sender gets the information that the message was not sent.
					2. If the recipient is in the database, it is checked whether the sender is not on the recipient list.
					3. If the recipient does not have a sender on the black list, then the message is sent.
					4. If the black list is empty, it is checked if the recipient is logged in. Not logged in, it is
						the message goes to the buffer. If the buffer overflows, messages will be sent to the IPC queue. As a customer is logged in
						the message is sent to him.
					5. The sender receives information if the message was sent / transferred to the buffer, or rejected - the rejection is then,
						when either there is no customer in the group or all members of the group have blocked the client.
				*/
				case MSGG :
					printf("Message: %s to %s\n", buffer.userSender, buffer.userRecipient);
					temp_send__msg_group_member = FALSE;
					if(currentGroupID != -1){
						if(groups[currentGroupID].clientNumber == 0){
							setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
							printf("Message send failed.\n");
						}
						else{
							buffer.type = RMSG;
							time(&buffer.timeSend);
							for(i = 0; i < groups[currentGroupID].clientNumber; i++){
								reciverID = getUserId(users[groups[currentGroupID].clientIn[i]].name, clientsNumber);
								printf("--->>Recipient %s status: %d.\n", users[groups[currentGroupID].clientIn[i]].name, 
												users[reciverID].logged);
								if(reciverID != -1){
										if(users[reciverID].logged == FALSE){
											if(users[reciverID].idQueue == -1){
												users[reciverID].idQueue = msgget(IPC_PRIVATE, IPC_CREAT|0644);
											}
										}
										//Checking if the recipient has no sender on the blacklist.
										if((users[reciverID].blockedUsersNumber != 0) || (users[reciverID].blockedGroupsNumber != 0)){
											clientExist = FALSE;
											for(i = 0; i < users[reciverID].blockedUsersNumber; i++){
												if(users[reciverID].blockedUsersID[i] == currentClientID){
													clientExist = TRUE;
												}
											}
										
											if(clientExist == TRUE){
												setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
												printf("Message send failed. Client is blocked!\n");
											}
											else{
												for(i = 0; i < users[reciverID].blockedGroupsNumber; i++){
													for(j = 0; j < groupsNumber; j++){
														if(groups[j].clientNumber != 0){
															for(k = 0; k < groups[j].clientNumber; k++){
																if(groups[j].clientIn[i] == currentClientID){
																	if(users[reciverID].blockedGroupsID[i] == groups[j].idGroup){
																		clientExist = TRUE;
																		k = groups[j].clientNumber;
																		j = groupsNumber;
																		i = users[reciverID].blockedGroupsNumber;
																	}
																}
															}
														}
													}
												}
											
												if(clientExist == TRUE){
													setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
													printf("Message send failed. Client is blocked!\n");
												}
												else{
													temp_send__msg_group_member = TRUE;
													msgsnd(users[reciverID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
												}
											}
										}
										else{
											if((users[reciverID].logged == FALSE) && (msgBuffer[reciverID].msgInBuffer < 10)){
												// If the recipient is not logged in and the message buffer is not full, 
												// then save the message in the buffer
												strcpy(msgBuffer[reciverID].userSender[msgBuffer[reciverID].msgInBuffer], buffer.userSender);
												strcpy(msgBuffer[reciverID].message[msgBuffer[reciverID].msgInBuffer], buffer.message);
												time(&msgBuffer[reciverID].timeSend[msgBuffer[reciverID].msgInBuffer]);
												msgBuffer[reciverID].msgPriority[msgBuffer[reciverID].msgInBuffer] = buffer.msgPriority;
												++msgBuffer[reciverID].msgInBuffer;
												tempTesting1 = msgBuffer[reciverID].msgInBuffer;
												// If the buffer is overflowing then messages in the correct order (relative to priority) are
												// sent to the appropriate IPC queue
												if(msgBuffer[reciverID].msgInBuffer == 7){
													for(i = 0; i < 10; i++){
														helpArryMsgBuffer[i] = -1;
													}
													l = 0;
													for(i = 0; i < msgBuffer[reciverID].msgInBuffer ; i++){
														for(j = 10; j > 0; j--){
															for(k = 0; k < 10; k++){
																if(msgBuffer[reciverID].msgPriority[k] == j){
																	helpArryMsgBuffer[l++] = k;
																}
															}
														}
													}
													l = 0;				
													for(i = 0; i < msgBuffer[reciverID].msgInBuffer ; i++){
														buffer.timeSend = msgBuffer[reciverID].timeSend[helpArryMsgBuffer[l]];
														setBuffer(&buffer, RMSG, msgBuffer[reciverID].userSender[helpArryMsgBuffer[l]], "", 
																			TRUE, msgBuffer[reciverID].message[helpArryMsgBuffer[l]], 1);
														msgsnd(users[reciverID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
														++l;
													}
													msgBuffer[reciverID].msgInBuffer = 0;
													for(j = 0; j < 10; j++){
														msgBuffer[reciverID].msgPriority[j] = -1;
													}
												}
												
												temp_send__msg_group_member = TRUE;
												printf("%d-", tempTesting1);
											}
											else{
												temp_send__msg_group_member = TRUE;
												msgsnd(users[reciverID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
											}
										}
								}
								if(temp_send__msg_group_member == TRUE){
									buffer.type = RECMSG;
									buffer.signal = TRUE;
									printf("Message send sucesful.\n");
								}
								else{
									buffer.type = RECMSG;
									buffer.signal = FALSE;
									printf("Message send failed.\n");
								}
							}
						}
					}
					else{
						setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
						printf("Message send failed.\n");
					}
					msgsnd(users[currentClientID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
					break;
				//***********************************************************************	
				/*
					SHOWALL, SHOWACTIVE, SHOWGROUPS, SHOWMEMBERS - serve as a display for all customers, active customers,
					all groups, members of a given group.
					If the client requests these commands, it passes the ID of the IPC queue he created (it is temporary).
					to which the requested information is sent.
				*/
				case SHOWALL :
					printf("Request SHOWALL from: %s\n", buffer.userSender);
					i = 0;
					tempIPCcurrentClient = buffer.signal;
					while(i < clientsNumber-1){
						setBuffer(&buffer, RMSG, buffer.userSender, "", FALSE, users[i].name, 0);
						msgsnd(tempIPCcurrentClient, &buffer, sizeof(message_buf)-sizeof(long), 0);
						++i;
					}
					setBuffer(&buffer, RMSG, buffer.userSender, "", TRUE, users[i].name, 0);
					msgsnd(tempIPCcurrentClient, &buffer, sizeof(message_buf)-sizeof(long), 0);
					break;
				//***********************************************************************	
				case SHOWACTIVE :
					printf("Request SHOWACTIVE from: %s\n", buffer.userSender);
					i = 0;
					tempIPCcurrentClient = buffer.signal;
					while(i < clientsNumber-1){
						if(users[i].logged == TRUE){
							setBuffer(&buffer, RMSG, buffer.userSender, "", FALSE, users[i].name, 0);
							msgsnd(tempIPCcurrentClient, &buffer, sizeof(message_buf)-sizeof(long), 0);
						}
						++i;
					}
					if(users[i].logged == TRUE){
						setBuffer(&buffer, RMSG, buffer.userSender, "", TRUE, users[i].name, 0);
					}
					else{
						setBuffer(&buffer, RMSG, buffer.userSender, "", TRUE, "", 0);
					}
					msgsnd(tempIPCcurrentClient, &buffer, sizeof(message_buf)-sizeof(long), 0);
					break;
				//***********************************************************************	
				case SHOWGROUPS :
					printf("Request SHOWGROUPS from: %s\n", buffer.userSender);
					i = 0;
					tempIPCcurrentClient = buffer.signal;
					while(i < groupsNumber-1){
						setBuffer(&buffer, RMSG, buffer.userSender, "", FALSE, groups[i].name, 0);
						msgsnd(tempIPCcurrentClient, &buffer, sizeof(message_buf)-sizeof(long), 0);
						++i;
					}
					setBuffer(&buffer, RMSG, buffer.userSender, "", TRUE, groups[i].name, 0);
					msgsnd(tempIPCcurrentClient, &buffer, sizeof(message_buf)-sizeof(long), 0);
					break;
				//***********************************************************************	
				case SHOWMEMBERS :
				printf("Request SHOWMEMBERS from: %s\n", buffer.userSender);
					tempIPCcurrentClient = buffer.signal;
					if(currentGroupID == -1){
						setBuffer(&buffer, RMSG, buffer.userSender, "", TRUE, "", 0);
					}
					else{
						i = 0;
						while(i < groups[currentGroupID].clientNumber){
							setBuffer(&buffer, RMSG, buffer.userSender, "", FALSE, 
																users[groups[currentGroupID].clientIn[i]].name, 0);
							msgsnd(tempIPCcurrentClient, &buffer, sizeof(message_buf)-sizeof(long), 0);
							++i;
						}
						setBuffer(&buffer, RMSG, buffer.userSender, "", TRUE, "", 0);
					}
					msgsnd(tempIPCcurrentClient, &buffer, sizeof(message_buf)-sizeof(long), 0);
					break;
				//***********************************************************************
				/*
					Adding a customer to a group:
					1. Check if the customer is already in the group.
					2. If this is the server refuses to add, otherwise the client adds to the group.
				*/
				case JOIN:
					printf("Request JOIN: %s to %s\n", buffer.userSender, buffer.userRecipient);
					if(currentGroupID != -1){
						if(groups[currentGroupID].clientNumber == 0){
							groups[currentGroupID].clientIn[0] = currentClientID;
							++groups[currentGroupID].clientNumber;
							setBuffer(&buffer, RECMSG, buffer.userSender, "", TRUE, "", 0);
							printf("-->Client added.\n");
						}
						else{
							clientExist = FALSE;
							for(i = 0; i < groups[currentGroupID].clientNumber; i++){
								if(groups[currentGroupID].clientIn[i] == currentClientID){
									clientExist = TRUE;
								}
							}
							if(clientExist == TRUE){
								setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
								printf("Request dismissed. Client belongs to group.\n");
							}
							else{
								groups[currentGroupID].clientIn[groups[currentGroupID].clientNumber] = currentClientID;
								++groups[currentGroupID].clientNumber;
								setBuffer(&buffer, RECMSG, buffer.userSender, "", TRUE, "", 0);
								printf("-->Client added.\n");
							}
						}
					}
					else{
						setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
						printf("Request dismissed.\n");
					}
					msgsnd(users[currentClientID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
					break;
				//***********************************************************************
				/*
					Remove the customer from the group:
					1. Check if the customer is in the group.
					2. If it is a server, it removes the client, otherwise it refuses to execute.
				*/
				case LEAVE:
					printf("Request LEAVE: %s to %s\n", buffer.userSender, buffer.userRecipient);
					if(currentGroupID != -1){
						if(groups[currentGroupID].clientNumber == 0){
							setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
							printf("No clients in group.\n");
						}
						else{
							clientExist = FALSE;
							for(i = 0; i < groups[currentGroupID].clientNumber; i++){
								if(groups[currentGroupID].clientIn[i] == currentClientID){
									clientExist = TRUE;
									reciverID = i;
								}
							}
							if(clientExist == TRUE){
								groups[currentGroupID].clientIn[reciverID] = groups[currentGroupID].clientIn[reciverID+1];
								while(groups[currentGroupID].clientIn[reciverID] != -1){
									++reciverID;
									groups[currentGroupID].clientIn[reciverID] = groups[currentGroupID].clientIn[reciverID+1];
								}
								--groups[currentGroupID].clientNumber;
								setBuffer(&buffer, RECMSG, buffer.userSender, "", TRUE, "", 0);
								printf("Client remove from group.\n");
							}
							else{
								setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
								printf("Client no belongs to group.\n");
							}
						}
					}
					else{
						setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
						printf("Request dismissed.\n");
					}
					msgsnd(users[currentClientID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
					break;
				//***********************************************************************
				/*
					Adding a client to a blacklist:
					1. Check if the customer is already on the list.
					2. If this is the server refuses to add, otherwise it adds the client to the list.
				*/
				case BLACKLIST:
					printf("Request BLACKLIST %s to lock %s\n", buffer.userSender, buffer.userRecipient);
					reciverID = getUserId(buffer.userRecipient, clientsNumber);
					if(reciverID != -1){
						if(users[currentClientID].blockedUsersNumber == 0){
							users[currentClientID].blockedUsersID[0] = reciverID;
							++users[currentClientID].blockedUsersNumber;
							setBuffer(&buffer, RECMSG, buffer.userSender, "", TRUE, "", 0);
							printf("-->Client blocked.\n");
						}
						else{
							clientExist = FALSE;
							for(i = 0; i < users[currentClientID].blockedUsersNumber; i++){
								if(users[currentClientID].blockedUsersID[i] == reciverID){
									clientExist = TRUE;
								}
							}
							
							if(clientExist == TRUE){
								setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
								printf("Request dismissed. Client is blocked aleready.\n");
							}
							else{
								users[currentClientID].blockedUsersID[users[currentClientID].blockedUsersNumber] = reciverID;
								++users[currentClientID].blockedUsersNumber;
								setBuffer(&buffer, RECMSG, buffer.userSender, "", TRUE, "", 0);
								printf("-->Client blocked.\n");
							}
						}
					} 
					else{
						setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
						printf("Request dismissed.\n");
					}
					msgsnd(users[currentClientID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
					break;
				//***********************************************************************
				/*
					Adding the whole group to the blacklist:
					1. Check if the group is already on the list.
					2. If this is the server refuses to add, otherwise it adds the groups to the list.
				*/
				case BLACKLISTG:
					printf("Request BLACKLISTG %s to lock %s\n", buffer.userSender, buffer.userRecipient);
					if(currentGroupID != -1){
						if(users[currentClientID].blockedGroupsNumber == 0){
							users[currentClientID].blockedGroupsID[0] = currentGroupID;
							++users[currentClientID].blockedGroupsNumber;
							setBuffer(&buffer, RECMSG, buffer.userSender, "", TRUE, "", 0);
							printf("-->Group blocked.\n");
						}
						else{
							clientExist = FALSE;
							for(i = 0; i < users[currentClientID].blockedGroupsNumber; i++){
								if(users[currentClientID].blockedUsersID[i] == currentGroupID){
									clientExist = TRUE;
								}
							}
							
							if(clientExist == TRUE){
								setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
								printf("Request dismissed. Client is blocked aleready.\n");
							}
							else{
								users[currentClientID].blockedGroupsID[users[currentClientID].blockedGroupsNumber] = currentGroupID;
								++users[currentClientID].blockedGroupsNumber;
								setBuffer(&buffer, RECMSG, buffer.userSender, "", TRUE, "", 0);
								printf("-->Client blocked.\n");
							}
						}
					} 
					else{
						setBuffer(&buffer, RECMSG, buffer.userSender, "", FALSE, "", 0);
						printf("Request dismissed.\n");
					}
					msgsnd(users[currentClientID].idQueue, &buffer, sizeof(message_buf)-sizeof(long), 0);
					break;
			}
		}
	}
	/*
		If the server was given an order to end the operation, then before closing
		removes all IPC messages queued.
	*/
	for(i = 0; i < clientsNumber; i++){
		if(users[i].idQueue != -1){
			msgctl(users[i].idQueue, IPC_RMID, 0);
		}
	}
	msgctl(listenIPC, IPC_RMID, 0);
	return TRUE;
}


int main(){
	int shmexit = shmget(IPC_PRIVATE, sizeof(char), 0644|IPC_CREAT);		//Shared memory, to control the workpiece
	exitProgram = (int *) shmat(shmexit, NULL, 0);											//server processes.
	*exitProgram = FALSE;
	
	if(fork() > 0){
		char command[20];
		
		while(*exitProgram != TRUE){
			fgets(command, 20, stdin);						//Receiving a server termination message.
			command[strlen(command)-1] = '\0';
			if(strcmp(command, "exit") == 0){
				*exitProgram = TRUE;
				printf("Server is turn off\n");
			}
		}
	}
	else{
		if(customerService() == TRUE){
			printf("customerService exit successful.\n");
		}
		else{
			printf("customerService problem with exit!!!!\n");
		}
	}
	shmdt(exitProgram);
	shmctl(shmexit, IPC_RMID, 0);
	return 0;
}
