#include "project_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <ctime>
#include <algorithm>

/* Very minimal JSON-like parser helper for ProjectTemplate schema */
static std::string get_json_string(const std::string& json, const std::string& key) {
    std::string key_q = "\"" + key + "\"";
    size_t pos = json.find(key_q);
    if (pos == std::string::npos) return "";
    pos = json.find(":", pos + key_q.length());
    if (pos == std::string::npos) return "";
    pos = json.find("\"", pos + 1);
    if (pos == std::string::npos) return "";
    size_t end = json.find("\"", pos + 1);
    if (end == std::string::npos) return "";
    
    std::string val = json.substr(pos + 1, end - pos - 1);
    /* Handle simple escapes like \n */
    size_t e = 0;
    while ((e = val.find("\\n", e)) != std::string::npos) {
        val.replace(e, 2, "\n"); e++;
    }
    e = 0;
    while ((e = val.find("\\t", e)) != std::string::npos) {
        val.replace(e, 2, "\t"); e++;
    }
    return val;
}

static std::vector<std::string> get_json_array(const std::string& json, const std::string& key) {
    std::vector<std::string> results;
    std::string key_q = "\"" + key + "\"";
    size_t pos = json.find(key_q);
    if (pos == std::string::npos) return results;
    pos = json.find("[", pos);
    if (pos == std::string::npos) return results;
    size_t end_arr = json.find("]", pos);
    if (end_arr == std::string::npos) return results;

    std::string arr_content = json.substr(pos + 1, end_arr - pos - 1);
    size_t start = 0;
    while ((start = arr_content.find("\"", start)) != std::string::npos) {
        size_t end = arr_content.find("\"", start + 1);
        if (end == std::string::npos) break;
        results.push_back(arr_content.substr(start + 1, end - start - 1));
        start = end + 1;
    }
    return results;
}

std::vector<ProjectTemplate> ProjectManager::list_templates() {
    std::vector<ProjectTemplate> results;
    DIR* dir = opendir("templates");
    if (!dir) return results;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        if (filename.size() > 5 && filename.substr(filename.size() - 5) == ".json") {
            ProjectTemplate tmpl;
            if (load_template(filename.substr(0, filename.size() - 5), tmpl)) {
                results.push_back(tmpl);
            }
        }
    }
    closedir(dir);
    return results;
}

bool ProjectManager::load_template(const std::string& id, ProjectTemplate& out_tmpl) {
    std::string path = "templates/" + id + ".json";
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::stringstream buffer;
    buffer << f.rdbuf();
    std::string json = buffer.str();

    out_tmpl.id = id;
    out_tmpl.name = get_json_string(json, "name");
    out_tmpl.description = get_json_string(json, "description");
    out_tmpl.structure = get_json_array(json, "structure");

    /* Parse variables map (minimal) */
    size_t vpos = json.find("\"variables\"");
    if (vpos != std::string::npos) {
        size_t vstart = json.find("{", vpos);
        size_t vend = json.find("}", vstart);
        if (vstart != std::string::npos && vend != std::string::npos) {
            std::string vcontent = json.substr(vstart + 1, vend - vstart - 1);
            size_t cur = 0;
            while ((cur = vcontent.find("\"", cur)) != std::string::npos) {
                size_t k_end = vcontent.find("\"", cur + 1);
                if (k_end == std::string::npos) break;
                std::string k = vcontent.substr(cur + 1, k_end - cur - 1);
                size_t sep = vcontent.find(":", k_end);
                size_t v_start = vcontent.find("\"", sep);
                size_t v_end = vcontent.find("\"", v_start + 1);
                if (v_start != std::string::npos && v_end != std::string::npos) {
                    out_tmpl.variables[k] = vcontent.substr(v_start + 1, v_end - v_start - 1);
                    cur = v_end + 1;
                } else break;
            }
        }
    }

    /* Parse files array (objects) */
    size_t fpos = json.find("\"files\"");
    if (fpos != std::string::npos) {
        size_t fstart = json.find("[", fpos);
        size_t fend = json.find("]", fstart);
        if (fstart != std::string::npos && fend != std::string::npos) {
            std::string fcontent = json.substr(fstart + 1, fend - fstart - 1);
            size_t obj_start = 0;
            while ((obj_start = fcontent.find("{", obj_start)) != std::string::npos) {
                /* Find matching } by tracking depth */
                size_t obj_end = obj_start + 1;
                int depth = 1;
                while (depth > 0 && obj_end < fcontent.length()) {
                    if (fcontent[obj_end] == '{') depth++;
                    else if (fcontent[obj_end] == '}') depth--;
                    obj_end++;
                }
                if (depth > 0) break; 

                /* obj is the content between matching { and } */
                std::string obj = fcontent.substr(obj_start + 1, obj_end - obj_start - 2);
                
                ProjectFile pf;
                pf.target = get_json_string(obj, "target");
                pf.content = get_json_string(obj, "content");
                pf.type = get_json_string(obj, "type");
                if (pf.type == "") pf.type = "text";
                pf.source = get_json_string(obj, "source");
                pf.encoding = get_json_string(obj, "encoding");
                
                if (!pf.target.empty()) {
                    out_tmpl.files.push_back(pf);
                } else {
                    // std::cerr << "Debug: Skipping file entry with empty target. obj=" << obj << std::endl;
                }
                
                obj_start = obj_end + 1;
            }
        }
    }

    return true;
}

bool ProjectManager::create_project(const std::string& template_id, 
                                   const std::string& project_dir,
                                   const std::map<std::string, std::string>& variable_overrides,
                                   std::string& out_error) {
    ProjectTemplate tmpl;
    if (!load_template(template_id, tmpl)) {
        out_error = "Template not found: " + template_id;
        return false;
    }

    /* Build effective variable map */
    std::map<std::string, std::string> vars = tmpl.variables;
    for (auto const& [key, val] : variable_overrides) {
        vars[key] = val;
    }
    
    /* Inject auto tokens */
    time_t now = time(0);
    char dt[64];
    strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", localtime(&now));
    vars["DATE"] = dt;
    vars["CWD"] = project_dir;

    /* 1. Create structure */
    if (!ensure_dir(project_dir)) {
        out_error = "Could not create project root: " + project_dir;
        return false;
    }

    for (const auto& s : tmpl.structure) {
        if (!ensure_dir(project_dir + "/" + s)) {
            out_error = "Could not create directory: " + project_dir + "/" + s;
            return false;
        }
    }

    /* 2. Create files */
    for (const auto& f : tmpl.files) {
        std::string target_path = project_dir + "/" + f.target;
        
        if (f.type == "text") {
            std::string out_content = substitute_vars(f.content, vars);
            std::ofstream of(target_path);
            if (!of.is_open()) {
                out_error = "Could not write file: " + target_path;
                return false;
            }
            of << out_content;
            of.close();
        } 
        else if (f.type == "binary_inline") {
            std::vector<unsigned char> data = decode_base64(f.content);
            std::ofstream of(target_path, std::ios::binary);
            if (!of.is_open()) {
                out_error = "Could not write binary file: " + target_path;
                return false;
            }
            of.write((char*)data.data(), data.size());
            of.close();
        }
        else if (f.type == "binary_copy") {
            std::string src_path = "templates/resources/" + f.source;
            std::ifstream src(src_path, std::ios::binary);
            std::ofstream dst(target_path, std::ios::binary);
            if (!src.is_open() || !dst.is_open()) {
                out_error = "Could not copy resource: " + src_path;
                return false;
            }
            dst << src.rdbuf();
        }
    }

    return true;
}

std::string ProjectManager::substitute_vars(const std::string& input, 
                                           const std::map<std::string, std::string>& vars) {
    std::string result = input;
    for (auto const& [key, val] : vars) {
        std::string placeholder = "{{" + key + "}}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), val);
            pos += val.length();
        }
    }
    return result;
}

bool ProjectManager::ensure_dir(const std::string& path) {
    /* Simple mkdir -p equivalent for relative paths */
    std::string current_path = "";
    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, '/')) {
        if (item.empty()) continue;
        current_path += (current_path.empty() ? "" : "/") + item;
        mkdir(current_path.c_str(), 0755);
    }
    return true;
}

std::vector<unsigned char> ProjectManager::decode_base64(const std::string& input) {
    static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

    std::vector<unsigned char> ret;
    int i = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];

    while (in_ < (int)input.length() && input[in_] != '=') {
        char_array_4[i++] = input[in_++];
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = (unsigned char)base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) {
        int j;
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++)
            char_array_4[j] = (unsigned char)base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) ret.push_back(char_array_3[j]);
    }

    return ret;
}
