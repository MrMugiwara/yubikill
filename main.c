#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <usb.h>

#define YUBICO_VENDOR_ID 0x1050

#define ACTION_SHUTDOWN 0
#define ACTION_HIBERNATE 1


struct usb {
    char bus;
    char device;
};

struct config {
    char delay;
    char i3;
    char action;
};


/* Parses a config file of the form "key  value\n" */
int parse_config_file(char *filename, struct config *config) {
    FILE *fp;
    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "Config file not found. Using default config.\n");
        return -1;
    }
    char buffer[128];
    int line = 0;
    while (fgets(buffer, 128, fp) != NULL) {
        char key[64];
        char val[64];
        line++;

        if (buffer[0] == '#') continue;

        if (sscanf(buffer, "%s  %s", key, val) != 2) {
            fprintf(stderr, "Syntax error in config, line %d", line);
            fclose(fp);
            return -1;
        }

        if (!strcmp("i3", key)) {
            config->i3 = atoi(val);
        } else if (!strcmp("action", key)) {
            if (!strcmp("hibernate", val)) {
                config->action = ACTION_HIBERNATE;
            } else if (!strcmp("shutdown", val)) {
                config->action = ACTION_SHUTDOWN;
            } else {
                fprintf(stderr, "Invalid action defined in config file.");
                fclose(fp);
                return -1;
            }
        } else if (!strcmp("delay", key)) {
            config->delay = atoi(val);
        } else {
            fprintf(stderr, "Invalid key defined in config file");
            fclose(fp);
            return -1;
        }

    }

    fclose(fp);
    return 0;
}


/* loops over the attached usb devices and tries to find a yubikey */
int findYubikey(struct usb *yubikey) {
    struct usb_bus *bus;
    struct usb_device *dev;
    usb_init();
    usb_find_busses();
    usb_find_devices();
    for (bus = usb_busses; bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next){
            if (dev->descriptor.idVendor == YUBICO_VENDOR_ID) {
                yubikey->bus = atoi(bus->dirname);
                yubikey->device = atoi(dev->filename);
                return 0;
            }
        }
    }
    return -1;
}

/* sleep an amount of ms */
void sleepms(int ms) {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL); 
}

/* loop and periodically check that the yubikey is still attached */
int mainLoop(struct config *config, struct usb *yubikey) {
    struct usb yk;
    pid_t pid;

    while (1) {

        if (findYubikey(&yk) != 0) {
            /* yubikey was removed, initiate shutdown/hibernation */
            printf("[+] Yubikey removed! shutting down in %d seconds...\n", config->delay);

            if (config->i3) {
                /* display i3 message */
                if ((pid = fork()) < 0) {
                    fprintf(stderr, "Error when forking...");
                    return -1;
                } else if (pid == 0) {
                    char *args[] = { "i3-nagbar", "-m", "WARNING - yubikey was removed", NULL };
                    execv("/usr/bin/i3-nagbar", args);
                }
            }

            char shutdown = 1;
            for (int i = 0; i < 4 * (config->delay); i++) {
                sleepms(250);
                if (!findYubikey(&yk)) {
                    shutdown = 0;
                    printf("[+] Yubikey reinserted!\n");
                    break;
                }
            }

            if (config->i3) {
                /* remove i3 message */
                kill(pid, SIGTERM);
            }

            if (!shutdown) {
                continue;
            }

            if (config->action == ACTION_SHUTDOWN) {
                system("sudo poweroff -f");
            } else {
                system("sudo pm-hibernate");
            }
        }
        sleepms(250);
    }
}

void parse_args(int argc, char **argv, struct config *config, char **config_file) {
    int c;
    while ((c = getopt (argc, argv, "hsd:ic:")) != -1) {
        switch (c) {
            case 'c':
                *config_file = optarg;
                break;
            case 'h':
                config->action = ACTION_HIBERNATE;
                break;
            case 'p':
                config->action = ACTION_SHUTDOWN;
                break;
            case 'd':
                config->delay = atoi(optarg);
                break;
            case 'i':
                config->i3 = 1;
                break;
            default:
                printf("Usage: %s [args]\n", argv[0]);
                printf("\t -c\tpath to config file. If set, config loaded from file. \n");
                printf("\t\tIf not set, then program will load config from ~/.yubikill\n");
                printf("\t\tor use defaults if that file does not exist.\n");
                printf("\t\tNote that arguments are ignored when using a config file!\n");
                printf("\t -h\tHibernate if yubikey removed.\n");
                printf("\t -p\tPoweroff if yubikey removed. (default)\n");
                printf("\t -d\tSet the delay in seconds between hibernate/poweroff and yubikey removal. (default 0)\n");
                printf("\t -i\tDisplay warning on i3-nagbar if yubikey removed\n");
        }
    }
}

/* default config values */
void load_defaults(struct config *config) {
    config->i3 = 0;
    config->action = ACTION_SHUTDOWN;
    config->delay = 0;
}

/* tries to load the config from specified config file or ~/.yubikill */
int load_config(int argc, struct config *config, char *config_file) {
    int res = 0;

    if (config_file != NULL) {
        res = parse_config_file(config_file, config);
    } else if (argc < 2) {
        /* try ~/.yubikill only if no arguments defined */
        struct passwd *pw = getpwuid(getuid());
        const char *homedir = pw->pw_dir;
        int homelen = strlen(homedir);
        char *config_file = (char *) malloc(homelen + 14);
        strcpy(config_file, pw->pw_dir);
        strcat(config_file + homelen, "/.yubikill");
        res = parse_config_file(config_file, config);
    }

    return res;
}

int main(int argc, char **argv){
    struct usb yubikey;
    struct config config;
    char *config_file;
    load_defaults(&config);
    parse_args(argc, argv, &config, &config_file);
    if (load_config(argc, &config, config_file))
        return -1;

    if (findYubikey(&yubikey)) {
        printf("ERROR yubikey not found.");
        return -1;
    } else {
        printf("[+] Found Yubikey at bus %d/%d\n", yubikey.bus, yubikey.device);
    }

    mainLoop(&config, &yubikey);

    return 0;
}
