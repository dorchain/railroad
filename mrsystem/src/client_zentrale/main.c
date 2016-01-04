#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <config.h>
#include "zentrale.h"

#define SOFTWARE_VERSION "1.04"

static void usage(char *name)
{
   printf("mrzentrale V%s\nUsage:\n", SOFTWARE_VERSION);
   printf("%s ([-v] [-f] [-a <addr> | -i <iface>] -p <port> [-l path] [-z <zentrale>]) | -?\n", name);
   puts("-a - ip address of drehscheibe");
   puts("-i - interface to drehscheibe");
   puts("-p - port of drehscheibe");
   puts("-z - 0=mrsystem dont start zentrale, 1=start zentrale");
   puts("-f - dont fork to go in background");
   puts("-v - verbose");
   puts("-l - path where write lokomotive.cs2");
   puts("-? - this help");
}

int main(int argc, char *argv[])
{  ZentraleStruct *Zentrale;
   ConfigStruct *Config;
   pid_t ChildPid;
   time_t Now;
   int Ret, NumArgs;
   char **ValArgs;

   Config = ConfigCreate();
   if (Config != (ConfigStruct *)NULL)
   {
      NumArgs = argc;
      ValArgs = argv;
      ConfigInit(Config, MRSYSTEM_CONFIG_FILE);
      ConfigReadfile(Config);
      ConfigCmdLine(Config, "a:i:p:fs:vl:z:?", NumArgs, ValArgs);
      if (ConfigGetIntVal(Config, CfgUsageVal))
      {
         usage(argv[0]);
         Ret = 1;
      }
      else if (ConfigGetIntVal(Config, CfgForkVal))
      {
         ChildPid = fork();
         if (ChildPid == -1)
         {
            if (ConfigGetIntVal(Config, CfgVerboseVal))
               puts("ERROR: can not go to backgound");
            Ret = 4;
         }
         else if (ChildPid == 0)
         {
            if (ConfigGetIntVal(Config, CfgVerboseVal))
               puts("child running");
            Zentrale = ZentraleCreate();
            if (Zentrale != (ZentraleStruct *)NULL)
            {
               ZentraleInit(Zentrale,
                            ConfigGetIntVal(Config, CfgVerboseVal),
                            ConfigGetIntVal(Config, CfgZentraleVal),
                            ConfigGetStrVal(Config, CfgIfaceVal),
                            ConfigGetStrVal(Config, CfgAddrVal),
                            ConfigGetIntVal(Config, CfgPortVal),
                            ConfigGetStrVal(Config, CfgPathVal));
               ZentraleRun(Zentrale);
               ZentraleDestroy(Zentrale);
               Ret = 0;
            }
            else
            {
               Ret = 2;
            }
         }
         else
         {
            if (ConfigGetIntVal(Config, CfgVerboseVal))
               puts("parent terminates");
            signal(SIGCHLD, SIG_IGN);
            Ret = 0;
         }
      }
      else
      {
         Now = time(NULL);
         if (ConfigGetIntVal(Config, CfgVerboseVal))
            printf("start with no fork at %s\n", asctime(localtime(&Now)));
         Zentrale = ZentraleCreate();
         if (Zentrale != (ZentraleStruct *)NULL)
         {
            ZentraleInit(Zentrale,
                         ConfigGetIntVal(Config, CfgVerboseVal),
                         ConfigGetIntVal(Config, CfgZentraleVal),
                         ConfigGetStrVal(Config, CfgIfaceVal),
                         ConfigGetStrVal(Config, CfgAddrVal),
                         ConfigGetIntVal(Config, CfgPortVal),
                         ConfigGetStrVal(Config, CfgPathVal));
            ZentraleRun(Zentrale);
            ZentraleDestroy(Zentrale);
            Ret = 0;
         }
         else
         {
            Ret = 1;
         }
      }
      ConfigExit(Config);
      ConfigDestroy(Config);
   }
   else
   {
      Ret = 3;
   }
   return(Ret);
}