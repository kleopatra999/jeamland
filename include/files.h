/**********************************************************************
 * The JeamLand talker system
 * (c) Andy Fiddaman, 1994-96
 *
 * File:	files.h
 * Function:	Pathnames of system files / directories.
 **********************************************************************/

#define F_AUDIT			"secure/audit/"
#define F_EMAILD		"secure/email/"
#define F_VALEMAIL		F_EMAILD "valemail.o"
#define F_SENDME		"help/sendme/"
#define F_SNOOPS		"secure/snoops/"
#define F_SNOOPED		F_SNOOPS "snooped"
#define F_SITEBAN		"etc/banned"
#define F_INETD_HOSTS		"etc/inetd_hosts"
#define F_CDUDP_HOSTS		"etc/cdudp_hosts"
#define F_HOST_DUMP		"etc/hosts.dump"
#define F_FEELINGS		"etc/feelings"
#define F_MAILSPOOL		"etc/mailspool"
#define F_PID			"etc/pid"
#define F_ANGEL_PID		"etc/angel.pid"
#define F_MASTERSAVE		"etc/mastersave.o"
#define F_MBS			"etc/mbs.o"
#define F_SUDO			"etc/sudoers"
#define F_JLM			"etc/modules"
/* If this file exists, automatic purges will not take place. */
#define F_NOPURGE		"etc/nopurge"

/* For help files etc. */
#define F_INDEX			"__INDEX__"

/* These are in etc by default. */
#define F_WELCOME		"welcome"
#define F_LOGOUT		"logout"
#define F_MOTD			"motd"
#define F_CONDITIONS		"conditions"

/* The file that the sign program logs to */
#define F_SIGN_LOG		"log/secure/sign"

