struct DLModule_Info
{
   char *module_name;
   char *module_author;
   char *module_author_email;
   int major_version, minor_version;
};

void DLoader_Init(char *pathname);
int DLoader_Exec_Cmd(char *cmdbuf);
void CtdlRegisterCleanupHook(void *fcn_ptr);
void CtdlRegisterSessionHook(void *fcn_ptr, int StartStop);
void PerformSessionHooks(int EventType);
void CtdlRegisterProtoHook(void (*handler)(char *), char *cmd, char *desc);
struct DLModule_Info *Dynamic_Module_Init(void);
