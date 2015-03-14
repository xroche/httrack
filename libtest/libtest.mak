# Makefile

OBJDIR = ./Release/
PATH = $(DEVSTU)\vc\bin;$(DEVSTU)\sharedide\bin;$(PATH)
INCLUDES = /I "." /I ".." /I "../src" /I "$(DEVSTU)\vc\include" /I"../../openssl-1.0.1j\include\openssl"
LIB_FLAGS = /link /LIBPATH:"L:\HTTrack\httrack\src_win\libhttrack" /LIBPATH:"L:\HTTrack\httrack\libhttrack"
COMMON_FLAGS = /W3 /O2 /Fo"$(OBJDIR)" /Fd"$(OBJDIR)" /Fa"$(OBJDIR)" $(INCLUDES)
CPP_FLAGS = /LD $(COMMON_FLAGS) libhttrack.lib
BIN_FLAGS = /link /LIBPATH:"C:\temp\Debuglib"

all:
  cl $(CPP_FLAGS) \
    callbacks-example-simple.c -Fe$(OBJDIR)callbacks-example-simple.dll \
    $(LIB_FLAGS)
  cl $(CPP_FLAGS) \
    callbacks-example-log.c -Fe$(OBJDIR)callbacks-example-log.dll \
    $(LIB_FLAGS)
  cl $(CPP_FLAGS) \
    callbacks-example-baselinks.c -Fe$(OBJDIR)callbacks-example-baselinks.dll \
    $(LIB_FLAGS)
  cl $(CPP_FLAGS) \
    callbacks-example-contentfilter.c -Fe$(OBJDIR)callbacks-example-contentfilter.dll \
    $(LIB_FLAGS)
  cl $(CPP_FLAGS) \
    callbacks-example-displayheader.c -Fe$(OBJDIR)callbacks-example-displayheader.dll \
    $(LIB_FLAGS)
  cl $(CPP_FLAGS) \
    callbacks-example-filename.c -Fe$(OBJDIR)callbacks-example-filename.dll \
    $(LIB_FLAGS)
  cl $(CPP_FLAGS) \
    callbacks-example-filename2.c -Fe$(OBJDIR)callbacks-example-filename2.dll \
    $(LIB_FLAGS)
  cl $(CPP_FLAGS) \
    callbacks-example-filenameiisbug.c -Fe$(OBJDIR)callbacks-example-filenameiisbug.dll \
    $(LIB_FLAGS)
  cl $(CPP_FLAGS) \
    callbacks-example-changecontent.c -Fe$(OBJDIR)callbacks-example-changecontent.dll \
    $(LIB_FLAGS)
  cl $(CPP_FLAGS) \
    callbacks-example-listlinks.c -Fe$(OBJDIR)callbacks-example-listlinks.dll \
    $(LIB_FLAGS)
  cl $(COMMON_FLAGS) \
    example.c wsock32.lib libhttrack.lib -Fe$(OBJDIR)example.exe \
    $(BIN_FLAGS)
