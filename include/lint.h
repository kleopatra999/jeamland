/**********************************************************************
 * The JeamLand talker system.
 * (c) Andy Fiddaman, 1993-97
 *
 * File:	lint.h
 * Function:	All global function prototypes
 **********************************************************************/

/* Access.c */
int parse_banned_site(char *, unsigned long *, unsigned long *);
void dump_siteban_table(struct user *, int);
void add_siteban(unsigned long, unsigned long, int, char *);
int remove_siteban(unsigned long, unsigned long);
struct banned_site *sitebanned(unsigned long addr);

/* Alias.c */
void reset_eval_depth(void);
struct alias *create_alias(char *, char *);
struct alias *find_alias(struct alias *, char *);
int expand_alias(struct user *, struct alias *, int, char **, char **, char **);
void free_alias(struct alias *);
void add_alias(struct alias **, struct alias *);
void remove_alias(struct alias **, struct alias *);
int count_aliases(struct alias *);
void free_aliases(struct alias **);
void restore_alias(struct alias **, char *, char *, char *);
void alias_hash_stats(struct user *, int);
int valid_ralias(char *);
void store_galiases(void);
void alias_profile(struct user *, char *);

/* Access.c */
int offensive(char *);
int check_ip(char *, char *);

/* Backend.c */
void fatal(char *, ...);
void free_svalue(struct svalue *);
void log_file(char *, char *, ...);
void log_perror(char *, ...);
const char *perror_text(void);
char *type_name(struct svalue *);
void print_svalue(struct user *, struct svalue *);
int equal_svalue(struct svalue *, struct svalue *);
void update_current_time(void);
time_t calc_time_until(int, int);

/* Board.c */
struct board *restore_mailbox(char *);
struct board *restore_board(char *, char *);
int store_board(struct board *);
void free_board(struct board *);
struct message *create_message(void);
struct board *create_board(void);
void show_message(struct user *, struct board *, struct message *, int, void (*)(struct user *));
void add_message(struct board *, struct message *);
void remove_message(struct board *, struct message *);
char *get_headers(struct user *, struct board *, int, int);
struct message *find_message(struct board *, int);
char *prepend_re(char *);
char *quote_message(char *, char);
void reply_message(struct user *, struct message *, int);

/* Comm.c */
void show_terminals(struct user *);
void write_socket(struct user *, char *, ...);
void write_prompt(struct user *, char *, ...);
void fwrite_prompt(struct user *, char *, ...);
void fwrite_socket(struct user *, char *, ...);
void write_room(struct user *, char *, ...);
void write_room_wrt_uattr(struct user *, char *, ...);
void write_roomabu(struct user *, char *, ...);
void write_roomabu_wrt_uattr(struct user *, char *, ...);
void write_roomabu2(struct user *, struct user *, char *, ...);
void write_room_level(struct user *, int, char *, ...);
void write_room_levelabu(struct user *, int, char *, ...);
void tell_room(struct room *, char *, ...);
void tell_object_njlm(struct object *, char *, ...);
void write_all(char *, ...);
void write_level_wrt_attr(int, char *, char *, ...);
void write_level(int, char *, ...);
void write_levelabu(struct user *, int, char *, ...);
void write_allabu(struct user *, char *, ...);
void notify_level(int, char *, ...);
void notify_level_wrt_flags(int, unsigned long, char *, ...);
void notify_levelabu(struct user *, int, char *, ...);
void notify_levelabu_wrt_flags(struct user *, int, unsigned long, char *, ...);
void print_prompt(struct user *);
int parse_colour(struct user *, char *, struct strbuf *);
void attr_colour(struct user *, char *);
void uattr_colour(struct user *, char *);
void reset(struct user *);
void linewrap(struct user *);
int dump_file(struct user *, char *, char *, enum dump_mode);
void write_a_jlm(struct jlm *, int, char *, ...);

/* Crash.c */
#ifdef CRASH_TRACE
void backtrace(char *, int);
void init_crash(void);
void add_function(char *, char *, int);
void end_function(void);
void add_function_line(int);
void add_function_arg(char *);
void add_function_addr(void (*)());
void check_fundepth(int);
#endif

/* Ed.c */
int ed_start(struct user *, void (*)(struct user *, int), int, int);

/* Event.c */
struct event *create_event();
int add_event(struct event *, void (*)(struct event *), int, char *);
void remove_event(int);
void remove_events_by_name(char *);
struct event *find_event(int);
void handle_event(void);

/* File.c */
char *read_file(char *);
int write_file(char *, char *);
int append_file(char *, char *);
int send_email(char *, char *, char *, char *, int, char *, ...);
int send_email_file(struct user *, char *);
int count_files(char *);
struct vector *get_dir(char *, int);
int file_time(char *);
int file_size(char *);
int exist_line(char *, char *);
void add_line(char *, char *, ...);
void remove_line(char *, char *);
int grep_file(char *, char *, struct strbuf *, int);

/* Grupe.c */
struct grupe_el *create_grupe_el();
void free_grupe_el(struct grupe_el *);
struct grupe *create_grupe();
void free_grupe(struct grupe *);
void add_grupe_el(struct grupe *, char *, long);
void remove_grupe_el(struct grupe *, struct grupe_el *);
struct grupe *add_grupe(struct grupe **, char *);
void remove_grupe(struct grupe **, struct grupe *);
struct grupe_el *find_grupe_el(struct grupe *, char *);
struct grupe *find_grupe(struct grupe *, char *);
int count_grupes(struct grupe *);
int count_grupe_els(struct grupe *);
void free_grupes(struct grupe **);
int expand_grupe(struct user *, struct vecbuf *, char *);
void store_grupes(FILE *, struct grupe *);
void restore_grupes(struct grupe **, char *);
int member_grupe(struct grupe *, char *, char *, int, int);
int member_sysgrupe(char *, char *);

/* Hash.c */
struct hash *create_hash(int, char *, int (*)(char *, int), int);
void free_hash(struct hash *);
int insert_hash(struct hash **, void *);
void *lookup_hash(struct hash *, char *);
void remove_hash(struct hash *, char *);
void hash_stats(struct user *, struct hash *, int);

/* Imud.c */
void imud_tell(struct user *, char *, char *);
void imud_who(struct user *, char *);
void imud_finger(struct user *, char *);

/* Imud3.c */
void i3_send_string(char *);
int i3_shutdown(void);
int i3_startup(void);
void i3_event_connect(struct event *ev);
void i3_init(void);
void i3_load(void);
void i3_save(void);
struct i3_channel *i3_find_channel(char *);
void i3_tune_channel(struct i3_channel *, int);
char *i3chan_stat(enum i3stats);
int i3_del_channels(void);
int i3_del_hosts(void);
void i3_hash_stats(struct user *, int);
void i3_cmd_who(struct user *, struct i3_host *);
void i3_cmd_finger(struct user *, struct i3_host *, char *);
void i3_cmd_locate(struct user *, char *);
void i3_cmd_tell(struct user *, struct i3_host *, char *, char *);
int i3_cmd_channel(struct user *, char *, char *);
struct i3_host *lookup_i3_host(char *, char *);
void i3_lostconn(struct tcpsock *);

/* Inetd.c */
struct host *lookup_inetd_host(char *);
struct host *lookup_cdudp_host(char *);
void inetd_mail(char *, char *, char *, char *, int);
void save_hosts(struct host *, char *);
void inetd_who(struct user *, struct host *);
void cdudp_who(struct user *, struct host *);
void inetd_finger(struct user *, struct host *, char *);
void cdudp_finger(struct user *, struct host *, char *);
void inetd_locate(struct user *, char *, int);
void inetd_tell(struct user *, struct host *, char *, char *);
void cdudp_tell(struct user *, struct host *, char *, char *);
int inter_channel(char *, int);
void inetd_import(struct user *, char *, char *);

/* Jlm.c */
struct jlm *create_jlm(void);
void free_jlm(struct jlm *);
struct jlm *find_jlm(char *);
struct jlm *find_jlm_in_env(struct object *, char *);
struct jlm *jlm_pipe(char *);
void attach_jlm(struct object *, char *);
void jlm_reply(struct jlm *);
void kill_jlm(struct jlm *);
void jlm_start_service(struct jlm *, char *);
void jlm_service_param(struct jlm *, char *, char *);
void jlm_end_service(struct jlm *);
void jlm_hash_stats(struct user *, int);
void closedown_jlms(void);
void cleanup_jlms(void);

/* Login.c */
int login_allowed(struct user *, struct user *);
void logon_name(struct user *);

/* Mapping.c */
struct mapping *allocate_mapping(int, int, int, char *);
void free_mapping(struct mapping *);
void expand_mapping(struct mapping **, int);
struct svalue *map_value(struct mapping *, struct svalue *);
int mapping_space(struct mapping **);
int remove_mapping(struct mapping *, struct svalue *);
int remove_mapping_string(struct mapping *, char *);
struct svalue *map_string(struct mapping *, char *);
void check_dead_map(struct mapping **);

/* Master.c */
void change_user_level(char *, int, char *);
int query_real_level(char *);
char *query_level_setter(char *);
int level_users(struct vecbuf *, int);
int count_level_users(int);
int count_all_users(void);
int valid_path(struct user *, char *, enum valid_mode);
int valid_email(char *);
void valid_room_name(struct user *, char *);
int secure_password(struct user *, char *, char *);
char *random_password(int);
char *crypted(char *);
void delete_user(char *);
void register_valemail_id(struct user *, char *);
int valemail_service(char *, char *);
void pending_valemails(struct user *);
int remove_any_valemail(char *);
int check_sudo(char *, char *, char *);
void list_sudos(struct user *, char *);

/* Mbs.c */
struct mbs *create_mbs(void);
void free_mbs(struct mbs *);
struct umbs *create_umbs(void);
void free_umbs(struct umbs *);
void add_mbs(struct mbs *);
void remove_mbs(struct mbs *);
void add_umbs(struct user *, struct umbs *);
void remove_umbs(struct user *, struct umbs *);
void load_mbs(void);
void store_mbs(void);
struct mbs *find_mbs(char *, int);
struct mbs *find_mbs_by_room(char *);
struct room *get_mbs_room(struct user *, struct mbs *);
struct room *get_current_mbs(struct user *, struct umbs **, struct mbs **);
time_t mbs_get_last_note_time(struct board *);
struct message *mbs_get_first_unread(struct user *, struct board *,
    struct umbs *, int *);
struct umbs *find_umbs(struct user *, char *, int);
void remove_bad_umbs(struct user *);
void notify_mbs(char *);
void free_umbses(struct user *);

/* More.c */
int more_start(struct user *, char *, void (*)(struct user *));

/* Object.c */
struct object *create_object(void);
int move_object(struct object *, struct object *);
void free_object(struct object *);
void add_to_env(struct object *, struct object *);
void remove_from_env(struct object *);
char *obj_name(struct object *);

/* Parse.c */
struct command *find_command(struct user *, char *);
void filter_obscenities(char *);
void parse_command(struct user *, char **);
void command_hash_stats(struct user *, int);

/* Room.c */
struct exit *create_exit(void);
void free_exit(struct exit *);
struct room *create_room(void);
struct room *restore_room(char *);
struct room *new_room(char *, char *);
struct room *find_room(char *);
void destroy_room(struct room *);
void reload_room(struct room *);
int store_room(struct room *);
void remove_exit(struct room *, struct exit *);
void add_exit(struct room *, struct exit *);
int handle_exit(struct user *, char *, int);
void init_room(struct room *);
void free_room(struct room *);
void tfree_room(struct room *);
struct room *get_entrance_room(void);
void delete_room(char *);

/* Sent.c */
struct sent *create_sent(void);
void free_sent(struct sent *);
void add_sent_to_export_list(struct object *, struct sent *);
void add_sent_to_inv_list(struct object *, struct sent *);
void remove_sent_from_inv_list(struct object *, struct sent *);
void enter_env(struct object *, struct object *);
void leave_env(struct object *);
void free_sentences(struct object *);
struct sent *find_sent(struct object *, char *);
struct sent *find_sent_cmd(struct user *, char *);
int sent_cmd(struct user *, char *, char *);
void dump_sent_list(struct user *p, struct strbuf *t);

/* Socket.c */
void remove_ipc(void);
void eor(struct user *);
void noecho(struct user *);
void echo(struct user *);
void replace_interactive(struct user *, struct user *);
int insert_command(struct tcpsock *, char *);
char *lookup_ip_name(struct user *);
char *lookup_ip_number(struct user *);
char *ip_name(struct tcpsock *);
char *ip_number(struct tcpsock *);
char *ip_numtoname(char *);
char *ip_addrtonum(unsigned long);
void send_rnotify(struct user *, char *);
int send_a_rnotify_msg(int, char *);
void closedown_rnotify(void);
void dump_rnotify_table(struct user *);
void send_udp_packet(char *, int, char *);
int send_erq(int, char *, ...);
int send_user_erq(char *, int, char *, ...);
void my_sleep(unsigned int);
void close_socket(struct user *);
void init_tcpsock(struct tcpsock *, int);
struct tcpsock *create_tcpsock(char *);
void free_tcpsock(struct tcpsock *);
void tfree_tcpsock(struct tcpsock *);
int write_tcpsock(struct tcpsock *, char *, int);
int scan_tcpsock(struct tcpsock *);
char *process_tcpsock(struct tcpsock *);
int connect_tcpsock(struct tcpsock *, char *, int);
void close_tcpsock(struct tcpsock *);
int close_fd(int);
char * scan_udpsock(struct udpsock *);
void disconnect_user(struct user *, int);
int start_supersnoop(char *);
void nonblock(int);

/* Soul.c */
char *pluralise_verb(char *);
struct feeling *exist_feeling(char *);
void feeling_hash_stats(struct user *, int);
char *expand_feeling(struct user *, int, char **, int);

/* Stack.c */
void dec_n_elems(struct stack *, int);
void dec_stack(struct stack *);
void pop_n_elems(struct stack *, int);
void pop_stack(struct stack *);
void push_malloced_string(struct stack *, char *);
void push_number(struct stack *, long);
void push_unsigned(struct stack *, unsigned long);
void push_pointer(struct stack *, void *);
void push_fpointer(struct stack *, void (*)());
void push_string(struct stack *, char *);
void clean_stack(struct stack *);
void init_stack(struct stack *);
struct svalue *stack_svalue(struct stack *, int);
void dump_stack(struct user *, struct stack *);

/* String.c */
void my_strncpy(char *, char *, int);
void deltrail(char *);
void delheadtrail(char **);
char *nctime(time_t *);
char *shnctime(time_t *);
char *vshnctime(time_t *);
char *string_copy(char *, char *);
char *capitalise(char *);
char *lower_case(char *);
char *capfirst(char *);
char *conv_time(time_t);
void shrink_strbuf(struct strbuf *, int);
void init_strbuf(struct strbuf *, int, char *);
void reinit_strbuf(struct strbuf *);
void cadd_strbuf(struct strbuf *, char);
void add_strbuf(struct strbuf *, char *);
void binary_add_strbuf(struct strbuf *, char *, int);
void sadd_strbuf(struct strbuf *, char *, ...);
void free_strbuf(struct strbuf *);
#ifdef STRBUF_STATS
void pop_strbuf(struct strbuf *);
#else
#define pop_strbuf(xx) (void)0
#endif /* STRBUF_STATS */
char *svalue_to_string(struct svalue *);
struct svalue *string_to_svalue(char *, char *);
struct svalue *decode_one(char *, char *);
char *code_string(char *);
char *decode_string(char *, char *);
struct vector *parse_range(struct user *, char *, int);
void expand_user_list(struct user *, char *, struct vecbuf *, int, int, int);
char *parse_chevron(char *);
int exist_cookie(char *, char);
char *parse_cookie(struct user *, char *);
char *parse_chevron_cookie(struct user *, char *);
int valid_name(struct user *, char *, int);
struct strlist *create_strlist(char *);
void free_strlist(struct strlist *);
void free_strlists(struct strlist **);
void add_strlist(struct strlist **, struct strlist *, enum sl_mode);
void remove_strlist(struct strlist **, struct strlist *);
void write_strlist(FILE *, char *, struct strlist *);
struct strlist *decode_strlist(char *, enum sl_mode);
struct strlist *member_strlist(struct strlist *, char *);
int strlist_size(struct strlist *);
void xcrypt(char *);
char *xcrypted(char *);
void init_composite_words(struct strbuf *);
void add_composite_word(struct strbuf *, char *);
void end_composite_words(struct strbuf *);

/* User.c */
char *who_text(int);
int count_users(struct user *);
struct user *create_user(void);
struct user *dead_copy(char *);
struct user *with_user_common(struct user *, char *, struct strbuf *);
struct user *with_user(struct user *, char *);
struct user *with_user_msg(struct user *, char *);
struct user *find_user_common(struct user *, char *, struct strbuf *);
struct user *find_user(struct user *, char *);
struct user *find_user_msg(struct user *, char *);
struct user *find_user_absolute(struct user *, char *);
void move_user(struct user *, struct room *);
void free_user(struct user *);
void tfree_user(struct user *);
void init_user(struct user *);
int exist_user(char *);
int restore_user(struct user *, char *);
int save_user(struct user *, int, int);
void reposition(struct user *);
int deliver_mail(char *, char *, char *, char *, char *, int, int);
char *gender_list(void);
int gender_number(char *, int);
char *query_gender(int, int);
int level_number(char *);
char *level_name(int, int);
char *finger_text(struct user *, char *, int);
int iaccess_check(struct user *, int);
int access_check(struct user *, struct user *);
char *query_password(char *);
void newmail_check(struct user *);
char *flags(struct user *);
struct user *doa_start(struct user *, char *);
void doa_end(struct user *, int);
time_t *user_time(struct user *, time_t);
void lastlog(struct user *);

/* Vector.c */
void free_vector(struct vector *);
struct vector *allocate_vector(int, int, char *);
void expand_vector(struct vector **, int);
struct svalue *vector_svalue(struct vector *);
int member_vector(struct svalue *, struct vector *);
void init_vecbuf(struct vecbuf *, int, char *);
int vecbuf_index(struct vecbuf *);
struct vector *vecbuf_vector(struct vecbuf *);
void free_vecbuf(struct vecbuf *);

/* Xalloc.c */
void set_times(long *, long *, long *);
int xalloced(void *);
void *xalloc(size_t, char *);
void xfree(void *);
void *xrealloc(void *, size_t);

/****************************************************************************/ 

#ifndef HPUX
#if defined(__linux__) || defined(_AIX4) || defined(FREEBSD)
extern char *crypt(const char *, const char *);
#else
char *crypt(char *, char *);
#endif
#endif /* !HPUX */

#if defined(sun) || defined(BSD)
#ifndef sunc
#define memmove(a, b, c) bcopy(b, a, c)
#endif
#endif

