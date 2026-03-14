#include "catch.hpp"
#include "project_manager.h"
#include "sim_api.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

static bool directory_exists(const char* path) {
    struct stat info;
    if (stat(path, &info) != 0) return false;
    return (info.st_mode & S_IFDIR) != 0;
}

static void remove_dir_recursive(const char* path) {
    DIR* d = opendir(path);
    if (!d) return;
    struct dirent* p;
    while ((p = readdir(d))) {
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
        char buf[512];
        snprintf(buf, sizeof(buf), "%s/%s", path, p->d_name);
        struct stat st;
        if (!stat(buf, &st)) {
            if (S_ISDIR(st.st_mode)) remove_dir_recursive(buf);
            else unlink(buf);
        }
    }
    closedir(d);
    rmdir(path);
}

TEST_CASE("Project Templates - Logic Validation", "[templates][logic]") {
    SECTION("List Templates") {
        std::vector<ProjectTemplate> templates = ProjectManager::list_templates();
        // Should have at least the basic ones (6502-minimal, etc.)
        CHECK(templates.size() > 0);
        
        bool found_minimal = false;
        for (const auto& t : templates) {
            if (t.id == "6502-minimal") found_minimal = true;
        }
        CHECK(found_minimal == true);
    }

    SECTION("Create Project from Template") {
        const char* proj_dir = "test_generated_proj";
        if (directory_exists(proj_dir)) remove_dir_recursive(proj_dir);

        std::string err;
        std::map<std::string, std::string> vars;
        vars["PROJECT_NAME"] = "TestProj";

        bool ok = ProjectManager::create_project("6502-minimal", proj_dir, vars, err);
        REQUIRE(ok == true);
        CHECK(directory_exists(proj_dir) == true);

        // Verify some expected files exist
        char main_asm[512];
        snprintf(main_asm, sizeof(main_asm), "%s/src/main.asm", proj_dir);
        struct stat st;
        CHECK(stat(main_asm, &st) == 0);

        // Optional: Attempt to assemble the generated file
        sim_session_t* s = sim_create("6502");
        int asm_ok = sim_load_asm(s, main_asm);
        if (asm_ok == 0) {
            CHECK(sim_get_state(s) == SIM_READY);
        } else {
            WARN("Generated project failed to assemble. (KickAssembler might be missing)");
        }
        sim_destroy(s);

        remove_dir_recursive(proj_dir);
    }
}
