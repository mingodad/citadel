struct DLModule_Info
{
   char module_name[30];
   char module_author[30];
   char module_author_email[30];
   int major_version, minor_version;
};

typedef struct s_symtab
{
   char *fcn_name;
   char *server_cmd;
   char *info_msg;
   char *module_path;
   struct s_symtab *next;
} symtab;

void DLoader_Init(char *pathname, symtab **);
int DLoader_Exec_Cmd(char *cmdbuf);
void add_symbol(char *fcn_name, char *server_cmd, char *info_msg, symtab **);
void CtdlRegisterHook(void *fcn_ptr, int fcn_type);
