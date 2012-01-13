#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <netinet/in.h>

#define LOG_FILENAME "/tmp/flash-update.log"
#define LOCK_FILENAME "/tmp/flash-update.lock"
#define LOG_BUFFER_SIZE 256

void usage(void)
{
  fprintf(stderr,"This program should be only started from scpinstall\n");
}


int write_log_message(int h,char *text,int percentage)
{
  char buffer[LOG_BUFFER_SIZE];
  memset(buffer,0,LOG_BUFFER_SIZE);
  snprintf(buffer,LOG_BUFFER_SIZE,"%s:(%d%%)\n",text,percentage);
  return write(h,buffer,strlen(buffer));
}

int main(int argc,char **argv,char **env)
{
  char dirbuffer[24];
  char *exec_argv[24];
  pid_t pid;
  int status;
  int i,log_file,lock_file,install_firmware=0;
  struct stat statbuf;

  if(argc!=1)
    {
      usage();
      return 1;
    }
  if(!strcmp(argv[0],"FIRMWARE INSTALLER"))
    install_firmware=1;
  else
    {
      if(strcmp(argv[0],"CONFIGURATION INSTALLER"))
	{
	  usage();
	  return 1;
	}
    }
  if(!getcwd(dirbuffer,24)
     ||strncmp(dirbuffer,"/var/tmp/scpinst-",17))
    {
      usage();
      return 1;
    }
  signal(SIGCHLD,SIG_DFL);
  setsid();
  close(0);
  close(1);
  close(2);
  open("/dev/null",O_WRONLY);
  open("/dev/null",O_WRONLY);
  open("/dev/null",O_WRONLY);
  if(install_firmware)
    {
      lock_file=open(LOCK_FILENAME,O_RDWR|O_CREAT|O_EXCL,0600);
      if(lock_file<0)
	return 1;
      close(lock_file);
      log_file=open(LOG_FILENAME,O_RDWR|O_CREAT|O_EXCL,0600);
      if(log_file<0)
	return 1;
      write_log_message(log_file,"Starting update",0);
      /* firmware installation */

      /* check for mandatory files */
      if(chdir("update"))
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}
      if(stat("download.bit",&statbuf)||(statbuf.st_size==0))
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}
      if(stat("dt.dtb",&statbuf)||(statbuf.st_size==0))
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}
      if(stat("linux.bin.gz",&statbuf)||(statbuf.st_size==0))
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}
      if(stat("romfs.bin.gz",&statbuf)||(statbuf.st_size==0))
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}

      /* extract identity information */
      exec_argv[0]="/bin/mbbl-imagetool";
      exec_argv[1]="-s";
      exec_argv[2]="-0";
      exec_argv[3]="-I";
      exec_argv[4]="/dev/mtd0";
      exec_argv[5]="-i";
      exec_argv[6]="identity.txt";
      exec_argv[7]=NULL;

      write_log_message(log_file,"Reading identity",3);

      pid=vfork();
      if(pid<0)
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}
      if(pid==0)
	{
	  execve(exec_argv[0],exec_argv,env);
	  _exit(1);
	}
      if((waitpid(pid,&status,0)!=pid)
	 ||!WIFEXITED(status)
	 ||(WEXITSTATUS(status)!=0))
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}
      
      if(stat("identity.txt",&statbuf)||(statbuf.st_size==0))
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}

      /* all mandatory files present, build the command line for installer */

      exec_argv[0]="/bin/mbbl-imagetool";
      exec_argv[1]="-s";
      exec_argv[2]="0x800000";
      exec_argv[3]="-o";
      exec_argv[4]="/dev/mtd0";
      exec_argv[5]="-b";
      exec_argv[6]="download.bit";
      exec_argv[7]="-d";
      exec_argv[8]="dt.dtb";
      exec_argv[9]="-k";
      exec_argv[10]="linux.bin.gz";
      exec_argv[11]="-r";
      exec_argv[12]="romfs.bin.gz";
      exec_argv[13]="-i";
      exec_argv[14]="identity.txt";
      i=15;

      /* optional files */
      if(!stat("mbbl.elf",&statbuf)&&(statbuf.st_size!=0))
	{
	  exec_argv[i++]="-e";
	  exec_argv[i++]="mbbl.elf";
	}
      if(!stat("logo-1.bin.gz",&statbuf)&&(statbuf.st_size!=0))
	{
	  exec_argv[i++]="-l";
	  exec_argv[i++]="logo-1.bin.gz";
	}

      if(!stat("8x12-font.bin.gz",&statbuf)&&(statbuf.st_size!=0))
	{
	  exec_argv[i++]="-f";
	  exec_argv[i++]="8x12-font.bin.gz";
	  if(!stat("16x24-font.bin.gz",&statbuf)&&(statbuf.st_size!=0))
	    {
	      exec_argv[i++]="-f";
	      exec_argv[i++]="16x24-font.bin.gz";
	    }
	}
      exec_argv[i]=NULL;

      write_log_message(log_file,"Writing",5);

      pid=vfork();
      if(pid<0)
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}
      if(pid==0)
	{
	  execve(exec_argv[0],exec_argv,env);
	  _exit(1);
	}
      if((waitpid(pid,&status,0)!=pid)
	 ||!WIFEXITED(status)
	 ||(WEXITSTATUS(status)!=0))
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}
      chdir("/");
      exec_argv[0]="/bin/mtd-storage";
      exec_argv[1]="-F";
      exec_argv[2]="-o";
      exec_argv[3]="/dev/mtd0";
      exec_argv[4]="-s";
      exec_argv[5]="0x800000";
      exec_argv[6]="-e";
      exec_argv[7]="0xffffff";
      exec_argv[8]="var/tmp/persist";
      exec_argv[9]=NULL;

      write_log_message(log_file,"Updating user area",80);

      pid=vfork();
      if(pid<0)
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}
      if(pid==0)
	{
	  execve(exec_argv[0],exec_argv,env);
	  _exit(1);
	}
      if((waitpid(pid,&status,0)!=pid)
	 ||!WIFEXITED(status)
	 ||(WEXITSTATUS(status)!=0))
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}

      write_log_message(log_file,"Finished",100);

      exec_argv[0]="/bin/avahi-daemon";
      exec_argv[1]="-k";
      exec_argv[2]=NULL;

      pid=vfork();
      if(pid<0)
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}
      if(pid==0)
	{
	  execve(exec_argv[0],exec_argv,env);
	  _exit(1);
	}
      waitpid(pid,&status,0);

      sleep(3);
      sync();
      int h;
      struct sockaddr_in serveraddr;
      FILE *f;

      serveraddr.sin_family=AF_INET;
      inet_aton("127.0.0.1",&serveraddr.sin_addr);
      serveraddr.sin_port=htons(2001);
      h=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
      if(h<0)
	{
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}

      if(connect(h,(struct sockaddr *)&serveraddr,
		 sizeof(serveraddr)))
	{
	  close(h);
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}
      f=fdopen(h,"r+");
      if(!f)
	{
	  close(h);
	  unlink(LOG_FILENAME);
	  unlink(LOCK_FILENAME);
	  return 1;
	}
      while(getc(f)!='\n');
      fputs("REBOOT REGULAR\n",f);
      fflush(f);
      while(getc(f)!='\n');
      unlink(LOG_FILENAME);
      fclose(f);
    }
  return 0;
}
