#include <dirent.h>
int main(){DIR*d=opendir("/user/app");return d?0:1;}