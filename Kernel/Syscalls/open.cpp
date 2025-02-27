/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Debug.h>
#include <Kernel/FileSystem/Custody.h>
#include <Kernel/FileSystem/FileDescription.h>
#include <Kernel/FileSystem/VirtualFileSystem.h>
#include <Kernel/Process.h>

namespace Kernel {

KResultOr<FlatPtr> Process::sys$open(Userspace<const Syscall::SC_open_params*> user_params)
{
    VERIFY_PROCESS_BIG_LOCK_ACQUIRED(this)
    Syscall::SC_open_params params;
    if (!copy_from_user(&params, user_params))
        return EFAULT;

    int dirfd = params.dirfd;
    int options = params.options;
    u16 mode = params.mode;

    if (options & O_NOFOLLOW_NOERROR)
        return EINVAL;

    if (options & O_UNLINK_INTERNAL)
        return EINVAL;

    if (options & O_WRONLY)
        REQUIRE_PROMISE(wpath);
    else if (options & O_RDONLY)
        REQUIRE_PROMISE(rpath);

    if (options & O_CREAT)
        REQUIRE_PROMISE(cpath);

    // Ignore everything except permission bits.
    mode &= 0777;

    auto path = get_syscall_path_argument(params.path);
    if (path.is_error())
        return path.error();

    dbgln_if(IO_DEBUG, "sys$open(dirfd={}, path='{}', options={}, mode={})", dirfd, path.value()->view(), options, mode);

    auto fd_or_error = m_fds.allocate();
    if (fd_or_error.is_error())
        return fd_or_error.error();
    auto new_fd = fd_or_error.release_value();
    RefPtr<Custody> base;
    if (dirfd == AT_FDCWD) {
        base = current_directory();
    } else {
        auto base_description = fds().file_description(dirfd);
        if (!base_description)
            return EBADF;
        if (!base_description->is_directory())
            return ENOTDIR;
        if (!base_description->custody())
            return EINVAL;
        base = base_description->custody();
    }

    auto result = VirtualFileSystem::the().open(path.value()->view(), options, mode & ~umask(), *base);
    if (result.is_error())
        return result.error();
    auto description = result.value();

    if (description->inode() && description->inode()->socket())
        return ENXIO;

    u32 fd_flags = (options & O_CLOEXEC) ? FD_CLOEXEC : 0;
    m_fds[new_fd.fd].set(move(description), fd_flags);
    return new_fd.fd;
}

KResultOr<FlatPtr> Process::sys$close(int fd)
{
    VERIFY_PROCESS_BIG_LOCK_ACQUIRED(this)
    REQUIRE_PROMISE(stdio);
    auto description = fds().file_description(fd);
    dbgln_if(IO_DEBUG, "sys$close({}) {}", fd, description.ptr());
    if (!description)
        return EBADF;
    int rc = description->close();
    m_fds[fd] = {};
    return rc;
}

}
