struct DLModule_Info
{
   char *module_name;
   char *module_author;
   char *module_author_email;
   int major_version, minor_version;
};

void DLoader_Init(char *pathname);
int DLoader_Exec_Cmd(char *cmdbuf);
void CtdlRegisterCleanupHook(void (*fcn_ptr)(void));
void CtdlRegisterSessionHook(void (*fcn_ptr)(void), int EventType);
void PerformSessionHooks(int EventType);
void PerformUserHooks(char *username, long usernum, int EventType);
void CtdlRegisterProtoHook(void (*handler)(char *), char *cmd, char *desc);
void CtdlRegisterUserHook(void (*fcn_ptr)(char*, long), int EventType);
struct DLModule_Info *Dynamic_Module_Init(void);
