#include "serial_library/testing.hpp"
#include <filesystem>

using namespace serial_library;

#if defined(USE_LINUX)

void LinuxTransceiverTest::SetUp()
{
    // find HOME directory
    char *homeVar = getenv("HOME");
    if(!homeVar)
    {
        SERLIB_LOG_ERROR("HOME could not be determined! Reverting to current directory");
        home = "";
        return;
    }

    home = std::string(homeVar);
    if(*home.end() != '/')
    {
        home += "/";
    }

    SERLIB_LOG_DEBUG("Got HOME directory as %s", home.c_str());
    
    // set up virtual serial ports
    char
        virtualFile1[24],
        virtualFile2[24],
        virtualSerialArg1[64],
        virtualSerialArg2[64];

    sprintf(virtualFile1, "%s/virtualsp1", homeDir().c_str());
    sprintf(virtualFile2, "%s/virtualsp2", homeDir().c_str());
    sprintf(virtualSerialArg1, "PTY,link=%s,raw,echo=0", virtualFile1);
    sprintf(virtualSerialArg2, "PTY,link=%s,raw,echo=0", virtualFile2);

    //delete files if they already exist
    if(std::filesystem::exists(virtualFile1))
    {
        std::filesystem::remove(virtualFile1);
    }

    if(std::filesystem::exists(virtualFile2))
    {
        std::filesystem::remove(virtualFile2);
    }
    
    // create a virtual terminal using socat
    pid_t proc = fork();
    if(proc < 0)
    {
        SERLIB_LOG_ERROR("fork() failed: %s", strerror(errno));
        exit(1);
    } else if(proc == 0)
    {
        // child process
        const char *socatCommand[] = {
            "/usr/bin/socat",
            virtualSerialArg1,
            virtualSerialArg2,
            NULL
        };

        if(execve(socatCommand[0], (char *const*) socatCommand, NULL) < 0)
        {
            SERLIB_LOG_ERROR("execve() failed: %s", strerror(errno));
        }
        exit(0);
    } else
    {
        //parent process
        socatProc = proc;

        //wait for file created by socat to appear
        while(!(std::filesystem::exists(virtualFile1) && std::filesystem::exists(virtualFile2)))
        {
            usleep(1000);
        }
    }
}


void LinuxTransceiverTest::TearDown()
{
    int sig = SIGINT;
    if(kill(socatProc, sig) < 0)
    {
        SERLIB_LOG_ERROR("kill() returned error: %s", strerror(errno));
    }

    
    if(waitpid(socatProc, NULL, 0) < 0)
    {
        SERLIB_LOG_ERROR("waitpid() failed: %s", strerror(errno));
    }
}


bool LinuxTransceiverTest::compareSerialData(const SerialData& data1, const SerialData& data2)
{
    if(data1.numData != data2.numData || memcmp(data1.data, data2.data, data1.numData) != 0)
    {
        SERLIB_LOG_INFO("data1 with length %d does not match data2 with length %d", data1.numData, data2.numData);
        SERLIB_LOG_INFO("data1: \"%s\", data2: \"%s\"", data1.data, data2.data);
        return false;
    }

    return true;
}


std::string LinuxTransceiverTest::homeDir()
{
    return home;
}

#endif
