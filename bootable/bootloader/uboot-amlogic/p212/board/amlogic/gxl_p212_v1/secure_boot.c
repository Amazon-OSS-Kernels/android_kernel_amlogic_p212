/*
 * secure_boot..c
 *
 * Copyright 2011-2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include "amzn_secure_boot.h"

const char *amzn_target_device_name(void)
{
	return "needle";
}


int amzn_target_device_type(void)
{
	return AMZN_ENGINEERING_DEVICE; //hard code to engineering device for now
}


