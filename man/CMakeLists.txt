#=============================================================================
#  LOS
#  Libre Octave Studio
#  $Id:$
#
#  Copyright (C) 2002-2006 by Werner Schweer and others
#  Copyright (C) 2012-2013 by Filipe Coelho
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#=============================================================================

#
# produce the manpages
#

configure_file(
    los.1.in
    ${PROJECT_BINARY_DIR}/man/${LOS_EXEC_NAME}.1
)

install(FILES 
    ${PROJECT_BINARY_DIR}/man/${LOS_EXEC_NAME}.1
    DESTINATION ${SHARE_INSTALL_PREFIX}/man/man1/
)
