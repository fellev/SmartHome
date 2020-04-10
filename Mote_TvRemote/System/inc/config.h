#ifndef CONFIG_H
#define CONFIG_H

struct configuration 
{
    byte check_virgin;
    byte nodeID;
    byte networkID;
    byte separator1;          //separators needed to keep strings from overlapping
    char description[10];
    byte separator2;
} CONFIG;

#endif