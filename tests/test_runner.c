#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "test_framework.h"
#include "../include/utils.h"
#include "../include/manifest.h"
#include "../include/registry.h"

static void test_trim_whitespace(void) {
    char s1[] = "  hello world  ";
    ASSERT_STR_EQ(trim_whitespace(s1), "hello world");

    char s2[] = "\"quoted string\"";
    ASSERT_STR_EQ(trim_whitespace(s2), "quoted string");

    char s3[] = "\t\n  ";
    ASSERT_STR_EQ(trim_whitespace(s3), "");
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

int main(void) {
    printf("\n=== Running Dotfiles Stow Manager C Unit Tests ===\n\n");

    RUN_TEST(test_trim_whitespace);
    RUN_TEST(test_string_array);
    RUN_TEST(test_manifest_load_save);
    RUN_TEST(test_registry_parsing);

    printf("\n=== Test Results: %d Passed, %d Failed ===\n\n",
           g_tests_run - g_tests_failed, g_tests_failed);

    return g_tests_failed == 0 ? 0 : 1;
}
