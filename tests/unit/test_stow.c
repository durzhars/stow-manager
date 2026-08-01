/*
 * Dotfiles Stow Manager (stow-manager)
 * Copyright (C) 2026 durzhars
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "../include/checker.h"
#include "../include/manifest.h"
#include "../include/stow.h"
#include "test_framework.h"

void test_stowignore(void)
{
    char tmp_dir[] = "/tmp/stow_ignore_XXXXXX";
    ASSERT(mkdtemp(tmp_dir) != NULL, "Should create temporary directory for stowignore test");

    char ignore_file[PATH_MAX * 2];
    snprintf(ignore_file, sizeof(ignore_file), "%s/.stowignore", tmp_dir);
    FILE *fp = fopen(ignore_file, "w");
    ASSERT(fp != NULL, "Should create .stowignore file");
    fprintf(fp, "# Comment line\nREADME.md\n*.bak\n");
    fclose(fp);

    StringArray patterns;
    str_array_init(&patterns);
    parse_stowignore(tmp_dir, &patterns);

    ASSERT(str_array_contains(&patterns, "README\\.md"),
           ".stowignore should contain escaped README\\.md");
    ASSERT(str_array_contains(&patterns, ".*\\.bak"),
           ".stowignore should contain escaped .*\\.bak");
    str_array_free(&patterns);

    char rm_cmd[PATH_MAX * 2];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", tmp_dir);
    (void)system(rm_cmd);
}

void test_dry_run_stow(void)
{
    char tmp_dotfiles[] = "/tmp/stow_dry_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_dry_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char pkg_dir[PATH_MAX * 4];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/mypkg", tmp_dotfiles);
    mkdir(pkg_dir, 0755);

    char cfg_file[PATH_MAX * 4];
    snprintf(cfg_file, sizeof(cfg_file), "%s/.configfile", pkg_dir);
    FILE *fp = fopen(cfg_file, "w");
    if (fp) {
        fprintf(fp, "test content\n");
        fclose(fp);
    }

    int res = stow_package(tmp_dotfiles, tmp_target, "mypkg", false, true);
    ASSERT(res == 0, "Dry run stow should return 0 success");

    char target_cfg[PATH_MAX * 4];
    snprintf(target_cfg, sizeof(target_cfg), "%s/.configfile", tmp_target);
    ASSERT(!file_exists(target_cfg), "Dry run stow must not modify disk or create symlinks");

    char cleanup_cmd[PATH_MAX * 4];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    (void)system(cleanup_cmd);
}

void test_symlink_health_check(void)
{
    char tmp_dotfiles[] = "/tmp/stow_sym_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_sym_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char broken_link[PATH_MAX * 4];
    snprintf(broken_link, sizeof(broken_link), "%s/broken.symlink", tmp_dotfiles);
    symlink("/nonexistent/file/path", broken_link);

    char del_pkg[PATH_MAX * 4];
    snprintf(del_pkg, sizeof(del_pkg), "%s/delpkg", tmp_dotfiles);
    mkdir(del_pkg, 0755);
    char del_file[PATH_MAX * 4];
    snprintf(del_file, sizeof(del_file), "%s/.dummy", del_pkg);

    char orphan_link[PATH_MAX * 4];
    snprintf(orphan_link, sizeof(orphan_link), "%s/orphan.symlink", tmp_target);
    symlink(del_file, orphan_link);

    check_symlink_health(tmp_dotfiles, tmp_target);

    char cleanup_cmd[PATH_MAX * 4];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    (void)system(cleanup_cmd);
}

void test_handle_mutual_exclusions(void)
{
    char tmp_dotfiles[] = "/tmp/stow_mut_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_mut_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char pkg_a_dir[PATH_MAX];
    char pkg_b_dir[PATH_MAX];
    snprintf(pkg_a_dir, sizeof(pkg_a_dir), "%s/pkgA", tmp_dotfiles);
    snprintf(pkg_b_dir, sizeof(pkg_b_dir), "%s/pkgB", tmp_dotfiles);
    ASSERT(mkdir(pkg_a_dir, 0755) == 0, "Should create pkgA dir");
    ASSERT(mkdir(pkg_b_dir, 0755) == 0, "Should create pkgB dir");

    char file_a[PATH_MAX];
    char file_b[PATH_MAX];
    snprintf(file_a, sizeof(file_a), "%s/.fileA", pkg_a_dir);
    snprintf(file_b, sizeof(file_b), "%s/.fileB", pkg_b_dir);

    FILE *fa = fopen(file_a, "w");
    if (fa) {
        fprintf(fa, "content A\n");
        fclose(fa);
    }
    FILE *fb = fopen(file_b, "w");
    if (fb) {
        fprintf(fb, "content B\n");
        fclose(fb);
    }

    // Manifest: pkgA conflicts with pkgB
    manifest_add_dep(tmp_dotfiles, "pkgA", "pkgB", "--conflict");

    // Stow pkgB first
    int res_b = stow_package(tmp_dotfiles, tmp_target, "pkgB", false, false);
    ASSERT(res_b == 0, "Stowing pkgB should succeed");
    ASSERT(is_package_stowed(tmp_target, tmp_dotfiles, "pkgB"), "pkgB should be stowed");

    // Handle mutual exclusions for pkgA
    handle_mutual_exclusions(tmp_target, tmp_dotfiles, "pkgA", false);

    // Verify pkgB was automatically unstowed
    ASSERT(!is_package_stowed(tmp_target, tmp_dotfiles, "pkgB"),
           "pkgB should be unstowed after mutual exclusion handling");

    char rm_cmd[PATH_MAX * 4];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    (void)system(rm_cmd);
}

void test_unfold_directory_symlinks(void)
{
    char tmp_dotfiles[] = "/tmp/stow_unf_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_unf_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char pkg_dir[PATH_MAX];
    char pkg_sub[PATH_MAX];
    char file1[PATH_MAX];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/mypkg", tmp_dotfiles);
    snprintf(pkg_sub, sizeof(pkg_sub), "%s/sub", pkg_dir);
    snprintf(file1, sizeof(file1), "%s/file1.txt", pkg_sub);

    ASSERT(mkdir(pkg_dir, 0755) == 0, "Should create pkg_dir");
    ASSERT(mkdir(pkg_sub, 0755) == 0, "Should create pkg_sub");
    FILE *f1 = fopen(file1, "w");
    if (f1) {
        fprintf(f1, "hello\n");
        fclose(f1);
    }

    // Target directory contains a directory symlink pointing inside dotfiles repo
    char target_sub[PATH_MAX];
    snprintf(target_sub, sizeof(target_sub), "%s/sub", tmp_target);
    ASSERT(symlink(pkg_sub, target_sub) == 0, "Should create directory symlink in target");

    ASSERT(is_symlink(target_sub), "target/sub should initially be a symlink");
    ASSERT(is_dir(target_sub), "target/sub should be a directory");

    // Run unfold_directory_symlinks
    unfold_directory_symlinks(tmp_target, tmp_dotfiles, false);

    // Verify symlink is unlinked and replaced with actual directory containing child symlink
    ASSERT(!is_symlink(target_sub), "target/sub should no longer be a symlink");
    ASSERT(is_dir(target_sub), "target/sub should still be a directory");

    char target_child[PATH_MAX];
    snprintf(target_child, sizeof(target_child), "%s/file1.txt", target_sub);
    ASSERT(is_symlink(target_child), "target/sub/file1.txt should be a symlink");

    char *sym_target = read_symlink_target(target_child);
    ASSERT(sym_target != NULL, "Symlink target should be readable");
    ASSERT_STR_EQ(sym_target, file1);
    free(sym_target);

    char rm_cmd[PATH_MAX * 4];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    (void)system(rm_cmd);
}

void test_package_stow_status(void)
{
    char tmp_dotfiles[] = "/tmp/stow_stat_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_stat_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char pkg_dir[PATH_MAX];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/statpkg", tmp_dotfiles);
    mkdir(pkg_dir, 0755);

    char f1[PATH_MAX];
    char f2[PATH_MAX];
    char f_deps[PATH_MAX];
    snprintf(f1, sizeof(f1), "%s/.file1", pkg_dir);
    snprintf(f2, sizeof(f2), "%s/.file2", pkg_dir);
    snprintf(f_deps, sizeof(f_deps), "%s/.stowdeps", pkg_dir);

    FILE *fp1 = fopen(f1, "w");
    if (fp1) {
        fprintf(fp1, "1\n");
        fclose(fp1);
    }
    FILE *fp2 = fopen(f2, "w");
    if (fp2) {
        fprintf(fp2, "2\n");
        fclose(fp2);
    }
    FILE *fpd = fopen(f_deps, "w");
    if (fpd) {
        fprintf(fpd, "REQUIRED=\"\"\n");
        fclose(fpd);
    }

    // Initially 0 files stowed
    ASSERT(get_package_stow_status(tmp_target, tmp_dotfiles, "statpkg") == STOW_STATUS_UNSTOWED,
           "Should be UNSTOWED initially");

    // Stow file1 only -> PARTIAL
    char tf1[PATH_MAX];
    snprintf(tf1, sizeof(tf1), "%s/.file1", tmp_target);
    symlink(f1, tf1);
    ASSERT(get_package_stow_status(tmp_target, tmp_dotfiles, "statpkg") == STOW_STATUS_PARTIAL,
           "Should be PARTIAL when 1/2 non-ignored files stowed");

    // Stow file2 as well -> STOWED (even though .stowdeps is not stowed)
    char tf2[PATH_MAX];
    snprintf(tf2, sizeof(tf2), "%s/.file2", tmp_target);
    symlink(f2, tf2);
    ASSERT(get_package_stow_status(tmp_target, tmp_dotfiles, "statpkg") == STOW_STATUS_STOWED,
           "Should be STOWED when 2/2 non-ignored files stowed");

    char rm_cmd[PATH_MAX * 4];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    (void)system(rm_cmd);
}

void test_native_stow_broken_symlink_conflict(void)
{
    char tmp_dotfiles[] = "/tmp/stow_brk_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_brk_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char pkg_dir[PATH_MAX * 2];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/brkpkg", tmp_dotfiles);
    ASSERT(mkdir(pkg_dir, 0755) == 0, "Should create brkpkg dir");

    char pkg_file[PATH_MAX * 2];
    snprintf(pkg_file, sizeof(pkg_file), "%s/.configfile", pkg_dir);
    FILE *fp = fopen(pkg_file, "w");
    if (fp) {
        fprintf(fp, "valid content\n");
        fclose(fp);
    }

    char target_link[PATH_MAX * 2];
    snprintf(target_link, sizeof(target_link), "%s/.configfile", tmp_target);
    ASSERT(symlink("/nonexistent/file/path/xyz", target_link) == 0,
           "Should create broken symlink in target");
    ASSERT(is_symlink(target_link), "target_link should be a symlink");

    int res = stow_package(tmp_dotfiles, tmp_target, "brkpkg", false, false);
    ASSERT(res == 0, "stow_package should succeed over broken symlink");

    ASSERT(is_symlink(target_link), "target_link should still be a symlink");
    char *sym_target = read_symlink_target(target_link);
    ASSERT(sym_target != NULL, "symlink target should be readable");
    ASSERT_STR_EQ(sym_target, pkg_file);
    free(sym_target);

    char cleanup_cmd[PATH_MAX * 4];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    (void)system(cleanup_cmd);
}

void test_native_unstow_edited_user_file(void)
{
    char tmp_dotfiles[] = "/tmp/stow_usr_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_usr_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char pkg_dir[PATH_MAX * 2];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/usrpkg", tmp_dotfiles);
    ASSERT(mkdir(pkg_dir, 0755) == 0, "Should create usrpkg dir");

    char pkg_file[PATH_MAX * 2];
    snprintf(pkg_file, sizeof(pkg_file), "%s/.usrconfig", pkg_dir);
    FILE *fp = fopen(pkg_file, "w");
    if (fp) {
        fprintf(fp, "original pkg content\n");
        fclose(fp);
    }

    ASSERT(stow_package(tmp_dotfiles, tmp_target, "usrpkg", false, false) == 0,
           "Stow should succeed");

    char target_file[PATH_MAX * 2];
    snprintf(target_file, sizeof(target_file), "%s/.usrconfig", tmp_target);
    ASSERT(is_symlink(target_file), "target_file should be a symlink initially");

    unlink(target_file);
    FILE *fusr = fopen(target_file, "w");
    if (fusr) {
        fprintf(fusr, "custom user file\n");
        fclose(fusr);
    }

    ASSERT(unstow_package(tmp_dotfiles, tmp_target, "usrpkg", false) == 0, "Unstow should succeed");

    ASSERT(file_exists(target_file), "User file should be preserved during unstow");
    ASSERT(!is_symlink(target_file), "Preserved file should not be a symlink");

    char cleanup_cmd[PATH_MAX * 4];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    (void)system(cleanup_cmd);
}

void test_native_unstow_recursive_directory_cleanup(void)
{
    char tmp_dotfiles[] = "/tmp/stow_rec_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_rec_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char pkg_dir[PATH_MAX * 2];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/recpkg", tmp_dotfiles);
    char deep_dir[PATH_MAX * 2];
    snprintf(deep_dir, sizeof(deep_dir), "%s/dir1/dir2/dir3", pkg_dir);
    ASSERT(mkdir_p(deep_dir, 0755) == 0, "Should create deep package directory");

    char pkg_file[PATH_MAX * 2];
    snprintf(pkg_file, sizeof(pkg_file), "%s/deep.conf", deep_dir);
    FILE *fp = fopen(pkg_file, "w");
    if (fp) {
        fprintf(fp, "deep config\n");
        fclose(fp);
    }

    ASSERT(stow_package(tmp_dotfiles, tmp_target, "recpkg", false, false) == 0,
           "Stow should succeed");

    char target_dir1[PATH_MAX * 2];
    char target_dir2[PATH_MAX * 2];
    char target_dir3[PATH_MAX * 2];
    char target_file[PATH_MAX * 2];
    snprintf(target_dir1, sizeof(target_dir1), "%s/dir1", tmp_target);
    snprintf(target_dir2, sizeof(target_dir2), "%s/dir1/dir2", tmp_target);
    snprintf(target_dir3, sizeof(target_dir3), "%s/dir1/dir2/dir3", tmp_target);
    snprintf(target_file, sizeof(target_file), "%s/dir1/dir2/dir3/deep.conf", tmp_target);

    ASSERT(is_symlink(target_file), "deep.conf should be stowed as symlink");

    char unrelated_file[PATH_MAX * 2];
    snprintf(unrelated_file, sizeof(unrelated_file), "%s/unrelated.txt", target_dir1);
    FILE *fur = fopen(unrelated_file, "w");
    if (fur) {
        fprintf(fur, "unrelated file\n");
        fclose(fur);
    }

    ASSERT(unstow_package(tmp_dotfiles, tmp_target, "recpkg", false) == 0, "Unstow should succeed");

    ASSERT(!file_exists(target_file), "deep.conf should be removed");
    ASSERT(!is_dir(target_dir3), "dir3 should be cleaned up as empty directory");
    ASSERT(!is_dir(target_dir2), "dir2 should be cleaned up as empty directory");
    ASSERT(is_dir(target_dir1), "dir1 should NOT be cleaned up because it contains unrelated.txt");
    ASSERT(file_exists(unrelated_file), "unrelated.txt in dir1 should exist");

    char cleanup_cmd[PATH_MAX * 4];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    (void)system(cleanup_cmd);
}

void test_native_stow_ignore_patterns(void)
{
    char tmp_dotfiles[] = "/tmp/stow_ign_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_ign_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char pkg_dir[PATH_MAX * 2];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/ignpkg", tmp_dotfiles);
    ASSERT(mkdir(pkg_dir, 0755) == 0, "Should create ignpkg dir");

    char ignore_file[PATH_MAX * 2];
    snprintf(ignore_file, sizeof(ignore_file), "%s/.stowignore", pkg_dir);
    FILE *fign = fopen(ignore_file, "w");
    if (fign) {
        fprintf(fign, "*.log\nignored_dir/\n");
        fclose(fign);
    }

    char normal_file[PATH_MAX * 2];
    snprintf(normal_file, sizeof(normal_file), "%s/normal.conf", pkg_dir);
    FILE *fnorm = fopen(normal_file, "w");
    if (fnorm) {
        fprintf(fnorm, "normal\n");
        fclose(fnorm);
    }

    char log_file[PATH_MAX * 2];
    snprintf(log_file, sizeof(log_file), "%s/app.log", pkg_dir);
    FILE *flog = fopen(log_file, "w");
    if (flog) {
        fprintf(flog, "log\n");
        fclose(flog);
    }

    char ign_subdir[PATH_MAX * 2];
    snprintf(ign_subdir, sizeof(ign_subdir), "%s/ignored_dir", pkg_dir);
    mkdir(ign_subdir, 0755);
    char secret_file[PATH_MAX * 2];
    snprintf(secret_file, sizeof(secret_file), "%s/secret.key", ign_subdir);
    FILE *fsec = fopen(secret_file, "w");
    if (fsec) {
        fprintf(fsec, "secret\n");
        fclose(fsec);
    }

    ASSERT(stow_package(tmp_dotfiles, tmp_target, "ignpkg", false, false) == 0,
           "Stow should succeed");

    char target_normal[PATH_MAX * 2];
    char target_log[PATH_MAX * 2];
    char target_secret[PATH_MAX * 2];
    snprintf(target_normal, sizeof(target_normal), "%s/normal.conf", tmp_target);
    snprintf(target_log, sizeof(target_log), "%s/app.log", tmp_target);
    snprintf(target_secret, sizeof(target_secret), "%s/ignored_dir/secret.key", tmp_target);

    ASSERT(is_symlink(target_normal), "normal.conf should be stowed");
    ASSERT(!file_exists(target_log), "app.log should be ignored and not created");
    ASSERT(!file_exists(target_secret), "ignored_dir/secret.key should be ignored and not created");

    char cleanup_cmd[PATH_MAX * 4];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    (void)system(cleanup_cmd);
}
