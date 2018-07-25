MYDIR=$(call get-my-dir)

LIB_UTILS_SRCS:=\
  $(MYDIR)/version.cpp\
  $(MYDIR)/sysclock.cpp\
  $(MYDIR)/log.cpp\
  $(MYDIR)/profiler.cpp\
  $(MYDIR)/scheduler.cpp\
  $(MYDIR)/time.cpp\
  $(MYDIR)/timer.cpp\

-include $(MYDIR)/$(shell $(CXX) -dumpmachine | sed "s/.*-//").mk

