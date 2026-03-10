#ifndef PROJECT_MANAGER_H
#define PROJECT_MANAGER_H

#include <string>
#include <vector>
#include <map>

struct ProjectFile {
    std::string target;
    std::string content;
    std::string type;     /* "text", "binary_inline", "binary_copy" */
    std::string source;   /* for binary_copy */
    std::string encoding; /* for binary_inline, e.g. "base64" */
};

struct ProjectTemplate {
    std::string id;
    std::string name;
    std::string description;
    std::map<std::string, std::string> variables;
    std::vector<std::string> structure;
    std::vector<ProjectFile> files;
};

class ProjectManager {
public:
    static std::vector<ProjectTemplate> list_templates();
    static bool load_template(const std::string& id, ProjectTemplate& out_tmpl);
    static bool create_project(const std::string& template_id, 
                               const std::string& project_dir,
                               const std::map<std::string, std::string>& variable_overrides,
                               std::string& out_error);

private:
    static std::string substitute_vars(const std::string& input, 
                                       const std::map<std::string, std::string>& vars);
    static bool ensure_dir(const std::string& path);
    static std::vector<unsigned char> decode_base64(const std::string& input);
};

#endif
