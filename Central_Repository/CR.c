//------------------------------------------------------------------------------------//
			//DBIN CENTRAL REPOSITORY Program//            
//------------------------------------------------------------------------------------//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <ctype.h>
#include <libgen.h>

// Port Definitions 
#define SU_IP_CR 8101
#define SU_SENDTO_CR 8104
#define NU_SENDTO_CR 8107
#define FSEE_PORT 8108
#define FBACK_PORT 8111
#define CR_REPLY_PORT 8113
#define TCP_FILE_TRANSFER_PORT 9000

#define MAX_CHUNK_SIZE 4096
#define MAX_IP_LENGTH 16
#define MAX_CMD_LENGTH 512
#define MAX_NODES 10
#define MAX_FILENAME_LENGTH 256
#define MAX_FILEPATH_LENGTH 512

// Global State
sqlite3 *G_DB;
pthread_mutex_t G_DB_MUTEX = PTHREAD_MUTEX_INITIALIZER;
volatile bool G_EXIT_REQUEST = false;
char G_IP_TABLE[MAX_NODES + 2][MAX_IP_LENGTH];
int G_NUM_NODES_IN_TABLE = 0;

// Structs for thread arguments
typedef struct { int port; bool is_su_listener; } listener_config;
typedef struct { char filename[MAX_FILENAME_LENGTH]; char sender_ip[MAX_IP_LENGTH]; } tcp_download_info;
typedef struct { char filename[MAX_FILENAME_LENGTH]; struct sockaddr_in requester_addr; int reply_port; } tcp_upload_info;

// Function Prototypes
void trim_whitespace(char *str);
bool initialize_database(const char* db_name);
void db_insert_file_record(const char* filename, const char* owner_ip);
void db_clear_all_records();
void parse_and_store_ip_table(const char* buffer);
bool is_ip_in_table(const char* ip_to_check);
void send_file_records(const struct sockaddr_in* recipient_addr, int reply_port, bool for_su);
void* tcp_download_thread(void* arg);
void* tcp_upload_thread(void* arg);
void* listener_thread_func(void* arg);

// Utility Functions (Database, IP, Validation)
void trim_whitespace(char *str) 
{
  if (!str) return;
  char *end = str + strlen(str) - 1;
  while (end >= str && isspace((unsigned char)*end)) end--;
  *(end + 1) = '\0';
  char *start = str;
  while (*start && isspace((unsigned char)*start)) start++;
  memmove(str, start, strlen(start) + 1);
}

bool initialize_database(const char* db_name) 
{
  if (sqlite3_open(db_name, &G_DB)) 
  {
    fprintf(stderr, "DB Error: %s\n", sqlite3_errmsg(G_DB)); 
    return false;
  }
  char *err_msg = 0;
  const char *sql = "CREATE TABLE IF NOT EXISTS StoredFiles (id INTEGER PRIMARY KEY, filename TEXT NOT NULL, owner_ip TEXT NOT NULL, UNIQUE(filename, owner_ip));";
  if (sqlite3_exec(G_DB, sql, 0, 0, &err_msg) != SQLITE_OK) 
  {
    fprintf(stderr, "SQL error: %s\n", err_msg); sqlite3_free(err_msg); 
    return false;
  }
  printf("Database initialized.\n");
  return true;
}

void db_insert_file_record(const char* filename, const char* owner_ip) 
{
  pthread_mutex_lock(&G_DB_MUTEX);
  const char* sql = "INSERT OR IGNORE INTO StoredFiles (filename, owner_ip) VALUES (?, ?);";
  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(G_DB, sql, -1, &stmt, 0) == SQLITE_OK) 
  {
    sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, owner_ip, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) fprintf(stderr, "DB insert failed: %s\n", sqlite3_errmsg(G_DB));
    else printf("DB record inserted for '%s'.\n", filename);
    sqlite3_finalize(stmt);
  }
  pthread_mutex_unlock(&G_DB_MUTEX);
}

void db_clear_all_records() 
{
  pthread_mutex_lock(&G_DB_MUTEX);
  char* err_msg = 0;
  if (sqlite3_exec(G_DB, "DELETE FROM StoredFiles;", 0, 0, &err_msg) != SQLITE_OK) 
  {
    fprintf(stderr, "Failed to clear records: %s\n", err_msg); sqlite3_free(err_msg);
  } 
  else 
  {
    printf("All file records cleared from database.\n");
  }
  pthread_mutex_unlock(&G_DB_MUTEX);
}

void parse_and_store_ip_table(const char* buffer) 
{
  char temp_buffer[1024];
  strncpy(temp_buffer, buffer, sizeof(temp_buffer));
  G_NUM_NODES_IN_TABLE = 0;
  strtok(temp_buffer, "\n");
  char* line = strtok(NULL, "\n");
  while (line != NULL && G_NUM_NODES_IN_TABLE < (MAX_NODES + 2)) 
  {
    trim_whitespace(line);
    if (strlen(line) > 0) 
    {
      strncpy(G_IP_TABLE[G_NUM_NODES_IN_TABLE], line, MAX_IP_LENGTH - 1);
      G_IP_TABLE[G_NUM_NODES_IN_TABLE][MAX_IP_LENGTH - 1] = '\0';
      G_NUM_NODES_IN_TABLE++;
    }
    line = strtok(NULL, "\n");
  }
}

bool is_ip_in_table(const char* ip_to_check) 
{
  for (int i = 0; i < G_NUM_NODES_IN_TABLE; ++i) 
  {
    if (strcmp(G_IP_TABLE[i], ip_to_check) == 0) return true;
  }
  return false;
}

// Command, Reply & TCP Transfer Functions
void send_file_records(const struct sockaddr_in* recipient_addr, int reply_port, bool for_su) 
{
  char recipient_ip[MAX_IP_LENGTH];
  inet_ntop(AF_INET, &recipient_addr->sin_addr, recipient_ip, sizeof(recipient_ip));
  char response_buffer[MAX_CHUNK_SIZE] = {0};
  sqlite3_stmt* stmt;
  const char* sql = for_su ? "SELECT filename, owner_ip FROM StoredFiles;" : "SELECT filename, owner_ip FROM StoredFiles WHERE owner_ip = ?;";

  pthread_mutex_lock(&G_DB_MUTEX);
  if (sqlite3_prepare_v2(G_DB, sql, -1, &stmt, 0) == SQLITE_OK) 
  {
    if (!for_su) sqlite3_bind_text(stmt, 1, recipient_ip, -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) 
    {
      char line[512];
      snprintf(line, sizeof(line), "File: %-40s | Owner: %s\n", sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1));
      strncat(response_buffer, line, sizeof(response_buffer) - strlen(response_buffer) - 1);
    }
    sqlite3_finalize(stmt);
  }
  pthread_mutex_unlock(&G_DB_MUTEX);

  if (strlen(response_buffer) == 0) strcpy(response_buffer, "No files found.\n");
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in reply_addr = *recipient_addr;
  reply_addr.sin_port = htons(reply_port);
  sendto(sock, response_buffer, strlen(response_buffer), 0, (struct sockaddr*)&reply_addr, sizeof(reply_addr));
  close(sock);
}

void* tcp_download_thread(void* arg) 
{
  tcp_download_info* info = (tcp_download_info*)arg;
  int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in listen_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(TCP_FILE_TRANSFER_PORT) };
  if (bind(listen_sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) < 0) 
  {
    perror("TCP download bind"); close(listen_sock); 
    free(info); 
    return NULL;
  }
  listen(listen_sock, 1);
    
  int data_sock = accept(listen_sock, NULL, NULL);
  close(listen_sock);
  if (data_sock < 0) 
  { 
    perror("TCP accept"); 
    free(info); 
    return NULL; 
  }

  mkdir("cr_data_storage", 0755);
  char save_path[MAX_FILEPATH_LENGTH];
  snprintf(save_path, sizeof(save_path), "cr_data_storage/%s_%s", info->sender_ip, info->filename);
  FILE* file = fopen(save_path, "wb");
  if (!file) 
  { 
    perror("fopen download"); 
    close(data_sock); 
    free(info); 
    return NULL; 
  }
    
  char buffer[MAX_CHUNK_SIZE];
  ssize_t bytes_received;
  while ((bytes_received = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) 
  {
    fwrite(buffer, 1, bytes_received, file);
  }
  fclose(file);
  close(data_sock);
  printf("File '%s' received and stored.\n", info->filename);
  db_insert_file_record(info->filename, info->sender_ip);
  free(info);
  return NULL;
}

void* tcp_upload_thread(void* arg) 
{
  tcp_upload_info* info = (tcp_upload_info*)arg;
  char requester_ip[MAX_IP_LENGTH];
  inet_ntop(AF_INET, &info->requester_addr.sin_addr, requester_ip, sizeof(requester_ip));
    
  char filepath[MAX_FILEPATH_LENGTH];
  snprintf(filepath, sizeof(filepath), "cr_data_storage/%s_%s", requester_ip, info->filename);

  int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  struct sockaddr_in listen_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(0) };
  if (bind(listen_sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) < 0) 
  {
    perror("TCP upload bind"); close(listen_sock); 
    free(info); 
    return NULL;
  }
  socklen_t addr_len = sizeof(listen_addr);
  getsockname(listen_sock, (struct sockaddr*)&listen_addr, &addr_len);
  int assigned_port = ntohs(listen_addr.sin_port);

  char reply[MAX_CMD_LENGTH];
  snprintf(reply, sizeof(reply), "READY_TO_SEND %s %d", info->filename, assigned_port);
  int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in reply_addr = info->requester_addr;
  reply_addr.sin_port = htons(info->reply_port);
  sendto(udp_sock, reply, strlen(reply), 0, (struct sockaddr*)&reply_addr, sizeof(reply_addr));
  close(udp_sock);

  listen(listen_sock, 1);
  int data_sock = accept(listen_sock, NULL, NULL);
  close(listen_sock);
  if (data_sock < 0) 
  { 
    perror("TCP accept"); 
    free(info); 
    return NULL; 
  }

  FILE* file = fopen(filepath, "rb");
  if (!file) 
  {
    close(data_sock); 
    free(info); 
    return NULL;
  }
  char buffer[MAX_CHUNK_SIZE];
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) 
  {
    if (send(data_sock, buffer, bytes_read, 0) < 0) 
    { 
      perror("TCP send"); 
      break; 
    }
  }
  fclose(file);
  close(data_sock);

  pthread_mutex_lock(&G_DB_MUTEX);
  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(G_DB, "DELETE FROM StoredFiles WHERE filename = ? AND owner_ip = ?;", -1, &stmt, 0) == SQLITE_OK) 
  {
    sqlite3_bind_text(stmt, 1, info->filename, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, requester_ip, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) fprintf(stderr, "DB delete failed: %s\n", sqlite3_errmsg(G_DB));
    else printf("DB record for '%s' deleted.\n", info->filename);
    sqlite3_finalize(stmt);
  }
  pthread_mutex_unlock(&G_DB_MUTEX);
  if (remove(filepath) == 0) printf("File '%s' deleted from disk.\n", filepath); 
  else perror("remove");
  
  free(info);
  return NULL;
}

// Listener logic
void* listener_thread_func(void* arg) 
{
  listener_config* config = (listener_config*)arg;
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in listen_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(config->port) };
  if (bind(sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) < 0) 
  {
    perror("listener bind"); free(config); return NULL;
  }
    
  printf("UDP Listener started on port %d.\n", config->port);
  char buffer[MAX_CMD_LENGTH];
  while (!G_EXIT_REQUEST) 
  {
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    ssize_t len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&sender_addr, &sender_len);
    if (len <= 0) continue;
        
    char sender_ip_str[MAX_IP_LENGTH];
    inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip_str, sizeof(sender_ip_str));
        
    if (!is_ip_in_table(sender_ip_str)) 
    {
      printf("SECURITY ALERT: Dropped packet from unauthorized IP: %s\n", sender_ip_str);
      continue;
    }
    buffer[len] = '\0';
    char buffer_copy[len + 1];
    memcpy(buffer_copy, buffer, len + 1);
    char* saveptr;
    char* command = strtok_r(buffer_copy, " ", &saveptr);
    if (!command) continue;

    if (strncmp(command, "REQUEST_UPLOAD", 14) == 0) 
    {
      char filename[MAX_FILENAME_LENGTH], up_sender_ip[MAX_IP_LENGTH];
      long long filesize;
      if (sscanf(buffer, "REQUEST_UPLOAD %s %lld %s", filename, &filesize, up_sender_ip) == 3) 
      {
        tcp_download_info* info = malloc(sizeof(tcp_download_info));
        strncpy(info->filename, filename, sizeof(info->filename) - 1);
        strncpy(info->sender_ip, up_sender_ip, sizeof(info->sender_ip) - 1);
        pthread_t download_tid;
        pthread_create(&download_tid, NULL, tcp_download_thread, info);
        pthread_detach(download_tid);
      }
    } 
    
    else if (config->is_su_listener) 
    {
      if (strcmp(command, "fsee") == 0) 
      { 
        send_file_records(&sender_addr, FSEE_PORT, true); 
      }
      else if (strcmp(command, "cleardb") == 0) 
      { 
        db_clear_all_records(); 
      }
      else if (strcmp(command, "fback") == 0) 
      {
        char* filename = strtok_r(NULL, "", &saveptr);
        if(filename) 
        {
          tcp_upload_info* info = malloc(sizeof(tcp_upload_info));
          strncpy(info->filename, filename, sizeof(info->filename) - 1);
          info->requester_addr = sender_addr;
          info->reply_port = FBACK_PORT;
          pthread_t upload_tid;
          pthread_create(&upload_tid, NULL, tcp_upload_thread, info);
          pthread_detach(upload_tid);
        }
      } 
      else if (strncmp(command, "Connection", 10) == 0) 
      {
        printf("\nTermination signal received. Shutting down server.\n"); exit(0);
      }
    } 
    else 
    { // Normal User Listener
      if (strcmp(command, "seemyfiles") == 0) 
      { 
        send_file_records(&sender_addr, CR_REPLY_PORT, false); 
      }
      else if (strcmp(command, "fback") == 0) 
      {
        char* filename = strtok_r(NULL, "", &saveptr);
        if(filename) 
        {
          tcp_upload_info* info = malloc(sizeof(tcp_upload_info));
          strncpy(info->filename, filename, sizeof(info->filename) - 1);
          info->requester_addr = sender_addr;
          info->reply_port = CR_REPLY_PORT;
          pthread_t upload_tid;
          pthread_create(&upload_tid, NULL, tcp_upload_thread, info);
          pthread_detach(upload_tid);
        }
      }
    }
  }
  close(sock);
  free(config);
  return NULL;
}

// Main
int main() 
{
  printf("Running Central Repository.\n");
  if (!initialize_database("repository.db")) return EXIT_FAILURE;
    
  char iptable_buffer[1024];
  int ip_sock = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in listen_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(SU_IP_CR) };
  if (bind(ip_sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) < 0) 
  {
    perror("bind for IP table"); 
    return EXIT_FAILURE;
  }
  printf("Waiting to receive IP table from Super User...\n");
  ssize_t len = recvfrom(ip_sock, iptable_buffer, sizeof(iptable_buffer) - 1, 0, NULL, NULL);
  close(ip_sock);
  if (len < 0) 
  { 
    perror("recvfrom IP table"); 
    return EXIT_FAILURE; 
  }
  iptable_buffer[len] = '\0';
  printf("IP Table received from Super User.\n");
  parse_and_store_ip_table(iptable_buffer);

  pthread_t su_tid, nu_tid;
  listener_config *su_config = malloc(sizeof(listener_config));
  su_config->port = SU_SENDTO_CR; su_config->is_su_listener = true;
  listener_config *nu_config = malloc(sizeof(listener_config));
  nu_config->port = NU_SENDTO_CR; nu_config->is_su_listener = false;

  pthread_create(&su_tid, NULL, listener_thread_func, su_config);
  pthread_create(&nu_tid, NULL, listener_thread_func, nu_config);

  printf("All services started. Repository is online.\n");

  pthread_join(su_tid, NULL);
  pthread_join(nu_tid, NULL);

  sqlite3_close(G_DB);
  printf("Shutting down.\n");
  return 0;
}
