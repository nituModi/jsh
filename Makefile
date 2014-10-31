# =============================================================
# This file is part of jsh.
# 
# jsh (jo-shell): A basic shell implementation
# Copyright (C) 2014 Jo Van Bulck <jo.vanbulck@student.kuleuven.be>
#
# jsh is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# jsh is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with jsh.  If not, see <http://www.gnu.org/licenses/>.
# ============================================================

JSH_INSTALL_DIR         = /usr/local/bin
MANPAGE_INSTALL_DIR     = /usr/local/share/man/man1
JSH_RELEASE_DIR         = ./JSH_$(RELEASE_NB)_RELEASE

DATE                    = $(shell date +%d\ %b\ %Y)
HOUR                    = $(shell date +%Hh%M)
MACHINE_BUILT           = $(shell uname -srm)
MACHINE_RELEASE         = $(shell uname -sm)
RELEASE_NB              = 1.1.0
RELEASE_VERSION_STR     = "jsh $(RELEASE_NB) on $(MACHINE_RELEASE) (official release $(DATE))"
DEV_BUILT_VERSION_STR   = "jsh post $(RELEASE_NB) on $(MACHINE_BUILT) \n(developer built $(DATE) : $(HOUR))"
VERSION_STR             = $(DEV_BUILT_VERSION_STR)
# 'make release' will override the above line to RELEASE_VERSION_STR

CC                      = gcc
CFLAGS                  = -g -DVERSION='$(VERSION_STR)' $(EXTRA_CFLAGS)
# EXTRA_CFLAGS is empty on default; 'make install' will add the INSTALL_CFLAGS
INSTALL_CFLAGS          = -DNODEBUG
LIBS                    = -lreadline
LN                      = $(CC) $(CFLAGS) jsh-common.o jsh.o alias.o jsh-parse.o jsh-completion.o -o jsh $(LIBS)

ECHO_LIBS               = echo "Linking jsh with the following libraries: $(LIBS) "

UNAME_S                 = $(shell uname -s)

ifeq ($(UNAME_S), Darwin)
	LINK = @$(LN) -L/usr/local/lib/ # Add library folder for Mac OS X readline (installed with homebrew)
else # try to link jsh with the readline library (and curses or termcap if needed)
	LINK = @(($(ECHO_LIBS)); ($(LN)) || (($(ECHO_LIBS) "lncurses"); $(LN) -lncurses) || \
	(($(ECHO_LIBS) "termcap"); $(LN) -termcap) || (echo "Failed linking jsh: all known fallback libraries were tried"))
endif

all: print_start_info jsh-common alias jsh link make_man_page
	@echo "-------- Compiling all done --------"

jsh-common: jsh-common.c jsh-common.h
	$(CC) $(CFLAGS) -c jsh-common.c -o jsh-common.o
alias: alias.c alias.h jsh-common.h
	$(CC) $(CFLAGS) -c alias.c -o alias.o
parse: jsh-parse.c jsh-parse.h jsh-common.h
	$(CC) $(CFLAGS) -c jsh-parse.c -o jsh-parse.o
completion: jsh-completion.h jsh-completion.c jsh-common.h
	$(CC) $(CFLAGS) -c jsh-completion.c -o jsh-completion.o
jsh: jsh.c jsh-common.h
	$(CC) $(CFLAGS) -c jsh.c -o jsh.o
link: jsh-common.o jsh.o alias.o jsh-parse.o jsh-completion.o
	$(LINK)

.PHONY: print_start_info
print_start_info:
	@echo "-------- making jsh version" $(VERSION_STR) "-------- "

.PHONY: make_man_page
make_man_page:
	@echo "adding version number and date to man-page jsh.1"
	@sed -e 's/@VERSION/$(RELEASE_NB)/' -e 's/@DATE/$(DATE)/' \
	-e '/@BEGIN_COMMENT/,/END_COMMENT/c\.\\" jsh manpage auto generated by jsh Makefile' \
	< jsh-man.1 > jsh.1

.PHONY: install
install:
	@echo "-------- installing jsh --------"
	@echo "making jsh with additional $(INSTALL_CFLAGS) flags"
	$(MAKE) --always-make EXTRA_CFLAGS='$(INSTALL_CFLAGS)'
	@echo "installing jsh executable in directory $(JSH_INSTALL_DIR)..."
	@test -d $(JSH_INSTALL_DIR) || (mkdir -p $(JSH_INSTALL_DIR) && echo "created directory $(JSH_INSTALL_DIR)")
	@install -m 0755 jsh $(JSH_INSTALL_DIR);
	
	@echo "installing the manpage in directory $(MANPAGE_INSTALL_DIR)..."
	@test -d $(MANPAGE_INSTALL_DIR) || (mkdir -p $(MANPAGE_INSTALL_DIR) && echo "created directory $(MANPAGE_INSTALL_DIR)")
	@install -m 0644 jsh.1 $(MANPAGE_INSTALL_DIR);
ifneq ($(UNAME_S), Darwin) # Man-DB update is not necessary on Mac
	@echo "updating man-db..."
	@mandb --quiet
endif
	@echo "-------- Installation all done --------"

.PHONY: uninstall
uninstall: 
	@echo  "uninstalling jsh from directories $(JSH_INSTALL_DIR) and $(MANPAGE_INSTALL_DIR)"
	rm -f $(JSH_INSTALL_DIR)/jsh $(MANPAGE_INSTALL_DIR)/jsh.1

.PHONY: release
release:
	@echo  "-------- making jsh $(RELEASE_NB) release built --------"
	$(MAKE) --always-make EXTRA_CFLAGS='$(INSTALL_CFLAGS)' VERSION_STR='$(RELEASE_VERSION_STR)'
	@echo "copying jsh executable in directory $(JSH_RELEASE_DIR)/ ..."
	@test -d $(JSH_RELEASE_DIR) || (mkdir -p $(JSH_RELEASE_DIR) && echo "created directory $(JSH_RELEASE_DIR)")
	cp jsh $(JSH_RELEASE_DIR) && chmod a+rx $(JSH_RELEASE_DIR)/jsh;
	
	@echo "copying the manpage in directory $(JSH_RELEASE_DIR)..."
	cp jsh.1 $(JSH_RELEASE_DIR) && chmod a+r $(JSH_RELEASE_DIR)/jsh.1;
	@echo "-------- Release built all done --------"

.PHONY: clean
clean:
	rm -f jsh-common.o alias.o jsh.o jsh
	(test -d $(JSH_RELEASE_DIR) && rm -rfI $(JSH_RELEASE_DIR)) || true
	
.PHONY: help
help:
	@echo "The following are valid targets for this Makefile:"
	@echo "... all        -- (the default if no target is provided); compiles the jo-shell to the 'jsh' binary in the current directory"
	@echo "... clean      -- removes all object files generated by the build process; also removes the $(JSH_RELEASE_DIR)/ directory and its content, if any"
	@echo "... install    -- installs the jsh binary with $(INSTALL_CFLAGS) options to $(JSH_INSTALL_DIR)/ and the jsh man page to $(MANPAGE_INSTALL_DIR)/ Make sure you have the necessary rights, use 'sudo make install' if necessary."
	@echo "... uninstall  -- removes the jsh binary from $(JSH_INSTALL_DIR)/ and the jsh man page from $(MANPAGE_INSTALL_DIR)/ Make sure you have the necessary rights, use 'sudo make uninstall' if necessary."
	@echo "... release    -- makes a jsh release built in $(JSH_RELEASE_DIR)/ in the current directory"

