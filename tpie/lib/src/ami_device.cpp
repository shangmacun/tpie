// Copyright (c) 1993 Darren Erik Vengroff
//
// File: ami_device.cpp
// Author: Darren Erik Vengroff <darrenv@eecs.umich.edu>
// Created: 8/22/93
//



static char ami_device_id[] = "$Id: ami_device.cpp,v 1.2 1994-09-29 13:25:28 darrenv Exp $";


#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <tpie_assert.h>
#include <ami_base.h>
#include <ami_device.h>


AMI_device::AMI_device(void)
{
    LOG_INFO("In AMI_device(void).\n");
}


AMI_device::AMI_device(unsigned int count, char **strings)
{
    char *s, *t;

    if (argc = count) {
        argv = new (char *)[argc];

        while (count--) {
            argv[count] = new char[strlen(strings[count]) + 1];
            for (s = strings[count], t = argv[count]; *t++ = *s++; )
                ;
        }
                     
    } else {
        argv = NULL;
    }
}


AMI_device::~AMI_device(void)
{
    dispose_contents();
}


const char * AMI_device::operator[](unsigned int index)
{
    return argv[index];
}

unsigned int AMI_device::arity()
{
    return argc;
}

void AMI_device::dispose_contents(void)
{
    if (argc) {
        while (argc--) {
            delete argv[argc];
        }

        tp_assert((argv != NULL), "Nonzero argc and NULL argv.");

        delete argv;
    }
}

AMI_err AMI_device::set_to_path(const char *path)
{
    const char *s, *t;
    unsigned int ii;

    dispose_contents();

    // Count the components
    for (argc = 1, s = path; *s; s++) {
        if (*s == ':') {
            argc++;
        }
    }
                
    argv = new (char *)[argc];

    // copy the components one by one.  t points to the start of the 
    // current component and s is used to scan to the end of it.

    for (ii = 0, s = t = path; ii < argc; ii++, t = ++s) {
        // Move past the current component.
        while (*s && (*s != ':')) 
            s++;

        tp_assert(((*s == ':') || (ii == argc - 1)),
                  "Path ended before all components found.");
        
        // Copy the current component.
        argv[ii] = new char[s - t + 1];
        strncpy(argv[ii], t, s - t);
        argv[ii][s - t] = '\0';
    }

    return AMI_ERROR_NO_ERROR;
}


AMI_err AMI_device::read_environment(const char *name)
{
    char *env_value = getenv(name);
    
    if (env_value == NULL) {
        return AMI_ERROR_ENV_UNDEFINED;
    }

    return set_to_path(env_value);
}


// Output of a device description:
ostream &operator<<(ostream &os, const AMI_device &dev)
{
    unsigned int ii;

    for (ii = 0; ii < dev.argc; ii++) {
        os << dev.argv[ii];
        if (ii < dev.argc - 1) {
            os << ':';
        }
    }
    
    return os;
}


