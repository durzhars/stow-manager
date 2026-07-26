#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "test_framework.h"
#include "../include/utils.h"
#include "../include/manifest.h"
#include "../include/registry.h"
#include "../include/stow.h"
#include "../include/checker.h"
#include "../include/config.h"

static void test_trim_whitespace(void) {
    char s1[] = "  hello world  ";
    ASSERT_STR_EQ(trim_whitespace(s1), "hello world");

    char s2[] = "\"quoted string\"";
    ASSERT_STR_EQ(trim_whitespace(s2), "quoted string");

    char s3[] = "\t\n  ";
    ASSERT_STR_EQ(trim_whitespace(s3), "");

    char s4[] = "\"";
    ASSERT_STR_EQ(trim_whitespace(s4), "\"");

    char s5[] = "'";
    ASSERT_STR_EQ(trim_whitespace(s5), "'");
}

static void test_mkdir_p(void) {
    char tmp_dir[] = "/tmp/stow_mkdir_XXXXXX";
    ASSERT(mkdtemp(tmp_dir) != NULL, "Should create temporary directory");

    char nested[1024];
    snprintf(nested, sizeof(nested), "%s/level1/level2/level3", tmp_dir);

    ASSERT(mkdir_p(nested, 0755) == 0, "mkdir_p should succeed creating nested directories");
    ASSERT(is_dir(nested), "Nested directory level3 should exist");

    char cleanup_cmd[2048];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\"", tmp_dir);
    system(cleanup_cmd);
}

static void test_path_normalization(void) {
    char p1[1024] = "/home/user/";
    normalize_path(p1);
    ASSERT_STR_EQ(p1, "/home/user");

    char p2[1024] = "/home/user///";
    normalize_path(p2);
    ASSERT_STR_EQ(p2, "/home/user");

    char p3[1024] = "/";
    normalize_path(p3);
    ASSERT_STR_EQ(p3, "/");

    char out[1024];
    join_path(out, sizeof(out), "/home/user/", "/.config/nvim");
    ASSERT_STR_EQ(out, "/home/user/.config/nvim");

    join_path(out, sizeof(out), "/home/user", ".config/nvim");
    ASSERT_STR_EQ(out, "/home/user/.config/nvim");

    ASSERT(is_path_prefix("/home/user/dotfiles/nvim", "/home/user/dotfiles"), "Prefix match should succeed");
    ASSERT(!is_path_prefix("/home/user/dotfiles-backup/file", "/home/user/dotfiles"), "Similar prefix without slash boundary must fail");
}

static void test_shell_escaping(void) {
    char esc[1024];
    escape_shell_arg("/home/user/my\"dir/$test", esc, sizeof(esc));
    ASSERT_STR_EQ(esc, "/home/user/my\\\"dir/\\$test");
}

static void test_expand_tilde(void) {
    char out[1024];
    expand_tilde_path("~/.zsh/plugins", out, sizeof(out));
    const char *home = getenv("HOME");
    if (home) {
        char expected[1024];
        snprintf(expected, sizeof(expected), "%s/.zsh/plugins", home);
        ASSERT_STR_EQ(out, expected);
    }
}

static void test_package_discovery(void) {
    char tmp_dir[] = "/tmp/stow_pkgdisc_XXXXXX";
    ASSERT(mkdtemp(tmp_dir) != NULL, "Should create temp dotfiles dir");

    char p_bin[1024], p_scripts[1024], p_build[1024], p_cfg[1024];
    snprintf(p_bin, sizeof(p_bin), "%s/bin", tmp_dir);
    snprintf(p_scripts, sizeof(p_scripts), "%s/scripts", tmp_dir);
    snprintf(p_build, sizeof(p_build), "%s/build", tmp_dir);
    snprintf(p_cfg, sizeof(p_cfg), "%s/.config", tmp_dir);

    mkdir_p(p_bin, 0755);
    mkdir_p(p_scripts, 0755);
    mkdir_p(p_build, 0755);
    mkdir_p(p_cfg, 0755);

    StringArray pkgs;
    str_array_init(&pkgs);
    get_all_packages(tmp_dir, &pkgs);

    ASSERT(str_array_contains(&pkgs, "bin"), "Package 'bin' should be discovered");
    ASSERT(str_array_contains(&pkgs, "scripts"), "Package 'scripts' should be discovered");
    ASSERT(str_array_contains(&pkgs, ".config"), "Package '.config' should be discovered");
    ASSERT(!str_array_contains(&pkgs, "build"), "Directory 'build' should be ignored");

    str_array_free(&pkgs);

    char cleanup_cmd[2048];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\"", tmp_dir);
    system(cleanup_cmd);
}

static void test_string_array(void) {
    StringArray arr;
    str_array_init(&arr);

    ASSERT(arr.count == 0, "Array count should initially be 0");
    str_array_append(&arr, "item1");
    str_array_append(&arr, "item2");

    ASSERT(arr.count == 2, "Array count should be 2");
    ASSERT(str_array_contains(&arr, "item1"), "Array should contain 'item1'");
    ASSERT(str_array_contains(&arr, "item2"), "Array should contain 'item2'");
    ASSERT(!str_array_contains(&arr, "item3"), "Array should not contain 'item3'");

    str_array_free(&arr);
    ASSERT(arr.count == 0, "Array count should be 0 after free");
}

static void test_manifest_load_save(void) {
    char tmp_dir[] = "/tmp/stow_test_XXXXXX";
    ASSERT(mkdtemp(tmp_dir) != NULL, "Should create temporary test directory");

    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/testpkg", tmp_dir);
    mkdir(pkg_dir, 0755);

    PackageManifest manifest;
    manifest_init(&manifest, "testpkg");
    str_array_append(&manifest.required, "bash");
    str_array_append(&manifest.required, "zsh");
    str_array_append(&manifest.optional, "fzf");
    str_array_append(&manifest.conflicts, "otherpkg");

    ASSERT(manifest_save(&manifest, tmp_dir), "Should save manifest file");
    manifest_free(&manifest);

    PackageManifest loaded;
    manifest_init(&loaded, "testpkg");
    ASSERT(manifest_load(&loaded, tmp_dir), "Should load manifest file");

    ASSERT(str_array_contains(&loaded.required, "bash"), "Loaded manifest should contain required 'bash'");
    ASSERT(str_array_contains(&loaded.required, "zsh"), "Loaded manifest should contain required 'zsh'");
    ASSERT(str_array_contains(&loaded.optional, "fzf"), "Loaded manifest should contain optional 'fzf'");
    ASSERT(str_array_contains(&loaded.conflicts, "otherpkg"), "Loaded manifest should contain conflict 'otherpkg'");

    manifest_free(&loaded);

    char cleanup_cmd[1024];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\"", tmp_dir);
    system(cleanup_cmd);
}

static void test_registry_parsing(void) {
    char tmp_dir[] = "/tmp/stow_reg_XXXXXX";
    ASSERT(mkdtemp(tmp_dir) != NULL, "Should create temporary test directory");

    char reg_path[1024];
    snprintf(reg_path, sizeof(reg_path), "%s/stow.registry", tmp_dir);
    FILE *fp = fopen(reg_path, "w");
    ASSERT(fp != NULL, "Should open registry file for writing");

    fprintf(fp, "tool_a = bin_a1 | bin_a2\n");
    fprintf(fp, "tool_a@ubuntu = pkg_a_ubuntu\n");
    fclose(fp);

    StringArray aliases;
    str_array_init(&aliases);
    registry_get_aliases(tmp_dir, "tool_a", &aliases);

    ASSERT(str_array_contains(&aliases, "bin_a1"), "Aliases should contain 'bin_a1'");
    ASSERT(str_array_contains(&aliases, "bin_a2"), "Aliases should contain 'bin_a2'");
    str_array_free(&aliases);

    char distro_pkg[256];
    registry_get_distro_pkg(tmp_dir, "tool_a", "ubuntu", distro_pkg, sizeof(distro_pkg));
    ASSERT_STR_EQ(distro_pkg, "pkg_a_ubuntu");

    char cleanup_cmd[1024];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\"", tmp_dir);
    system(cleanup_cmd);
}

static void test_dry_run_stow(void) {
    char tmp_dotfiles[] = "/tmp/stow_dry_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_dry_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/mypkg", tmp_dotfiles);
    mkdir(pkg_dir, 0755);

    char cfg_file[1024];
    snprintf(cfg_file, sizeof(cfg_file), "%s/.configfile", pkg_dir);
    FILE *fp = fopen(cfg_file, "w");
    if (fp) { fprintf(fp, "test content\n"); fclose(fp); }

    int res = stow_package(tmp_dotfiles, tmp_target, "mypkg", false, true);
    ASSERT(res == 0, "Dry run stow should return 0 success");

    char target_cfg[1024];
    snprintf(target_cfg, sizeof(target_cfg), "%s/.configfile", tmp_target);
    ASSERT(!file_exists(target_cfg), "Dry run stow must not modify disk or create symlinks");

    char cleanup_cmd[2048];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    system(cleanup_cmd);
}

static void test_symlink_health_check(void) {
    char tmp_dotfiles[] = "/tmp/stow_sym_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_sym_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char broken_link[1024];
    snprintf(broken_link, sizeof(broken_link), "%s/broken.symlink", tmp_dotfiles);
    symlink("/nonexistent/file/path", broken_link);

    char orphan_link[1024];
    snprintf(orphan_link, sizeof(orphan_link), "%s/orphan.symlink", tmp_target);
    char dummy_dot_target[1024];
    snprintf(dummy_dot_target, sizeof(dummy_dot_target), "%s/delpkg/.dummy", tmp_dotfiles);
    symlink(dummy_dot_target, orphan_link);

    check_symlink_health(tmp_dotfiles, tmp_target);

    char cleanup_cmd[2048];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    system(cleanup_cmd);
}

static void test_config_system(void) {
    Config cfg;
    config_init(&cfg);

    snprintf(cfg.config_file_path, sizeof(cfg.config_file_path), "/tmp/test_stow_config_%d", getpid());
    str_array_append(&cfg.dotfiles_dirs, "/home/user/my_dotfiles");
    snprintf(cfg.target_dir, sizeof(cfg.target_dir), "/home/user");

    ASSERT(config_save(&cfg), "Should save configuration file");
    config_free(&cfg);

    Config loaded;
    config_init(&loaded);
    snprintf(loaded.config_file_path, sizeof(loaded.config_file_path), "/tmp/test_stow_config_%d", getpid());

    ASSERT(config_load(&loaded), "Should load configuration file");
    ASSERT(loaded.dotfiles_dirs.count == 1, "Config should contain 1 dotfiles directory");
    ASSERT_STR_EQ(loaded.dotfiles_dirs.items[0], "/home/user/my_dotfiles");
    ASSERT_STR_EQ(loaded.target_dir, "/home/user");

    unlink(loaded.config_file_path);
    config_free(&loaded);
}

int main(void) {
    printf("\n=== Running Dotfiles Stow Manager C Unit Tests ===\n\n");

    RUN_TEST(test_trim_whitespace);
    RUN_TEST(test_mkdir_p);
    RUN_TEST(test_path_normalization);
    RUN_TEST(test_shell_escaping);
    RUN_TEST(test_expand_tilde);
    RUN_TEST(test_package_discovery);
    RUN_TEST(test_string_array);
    RUN_TEST(test_manifest_load_save);
    RUN_TEST(test_registry_parsing);
    RUN_TEST(test_dry_run_stow);
    RUN_TEST(test_symlink_health_check);
    RUN_TEST(test_config_system);

    printf("\n=== Test Results: %d Passed, %d Failed ===\n\n",
           g_tests_run - g_tests_failed, g_tests_failed);

    return g_tests_failed == 0 ? 0 : 1;
}
