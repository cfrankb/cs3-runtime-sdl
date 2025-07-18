#pragma once
#include <string>
#include <vector>

class CMenu;

class CMenuItem
{
public:
    enum
    {
        ITEM_STATIC,
        ITEM_RANGE,
        ITEM_OPTIONS,
        ROLE_NONE = 0,
    };

    CMenuItem(const std::string &tmpl, const int role = ROLE_NONE, CMenu *menu = nullptr);
    CMenuItem(const std::string &tmpl, const int rangeMin, const int rangeMax, int *value);
    CMenuItem(const std::string &tmpl, const std::vector<std::string> &options, int *value);
    ~CMenuItem();

    void disable(const bool value = true);
    bool isDisabled() const;
    void left();
    void right();
    int value() const;
    int role() const;
    CMenu *menu() const;
    void setRole(const int action = ROLE_NONE);
    std::string str() const;

protected:
    std::vector<std::string> m_options;
    std::string m_tmpl;
    int m_type = ITEM_STATIC;
    int *m_value = nullptr;
    int m_rangeMin = 0;
    int m_rangeMax = 0;
    bool m_disabled = false;
    int m_role = ROLE_NONE;
    CMenu *m_menu = nullptr;
};