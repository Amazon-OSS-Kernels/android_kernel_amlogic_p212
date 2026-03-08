/* Copyright 2017 Amazon.com, Inc. or its affiliates. All Rights Reserved. */
#ifndef AMZN_LOCKDOWN
#define AMZN_LOCKDOWN

/**
 * Issued when we enter interactive prompt so blacklisted
 * commands should be blocked
 */
void amzn_block_commands(void);

/**
 * Checks if the command can be allowed to run
 */
bool amzn_is_command_blocked(const char *cmd);

#endif
