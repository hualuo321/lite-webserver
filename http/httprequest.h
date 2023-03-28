#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>     
#include <mysql/mysql.h>  //mysql

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAII.h"

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,   // 正在解析请求行
        HEADERS,        // 将诶请求头
        BODY,           // 请求体
        FINISH,         // 完成
    };

    enum HTTP_CODE {    // 各种访问状态
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };
    
    HttpRequest() { Init(); }
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

    /* 
    todo 
    void HttpConn::ParseFormData() {}
    void HttpConn::ParseJson() {}
    */

private:
    bool ParseRequestLine_(const std::string& line);    // 解析请求行
    void ParseHeader_(const std::string& line);         // 解析请求头
    void ParseBody_(const std::string& line);           // 解析请求体

    void ParsePath_();                      // 解析路径
    void ParsePost_();                      // 解析POST
    void ParseFromUrlencoded_();            // 解析表单数据
    // 验证用户登陆注册
    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);

    PARSE_STATE state_;                                     // 解析状态
    std::string method_, path_, version_, body_;            // 请求方法
    std::unordered_map<std::string, std::string> header_;   
    std::unordered_map<std::string, std::string> post_;

    static const std::unordered_set<std::string> DEFAULT_HTML;  // 默认网页
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG; // 转换成16进制
    static int ConverHex(char ch);
};


#endif //HTTP_REQUEST_H