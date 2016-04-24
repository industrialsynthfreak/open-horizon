//
// open horizon -- undefined_darkness@outlook.com
//

#include "team_deathmatch.h"
#include "util/xml.h"

namespace game
{
//------------------------------------------------------------

inline int get_team(std::string t) { return t == "red" ? 1 : 0; }
inline std::string get_team(int t) { return t == 1 ? "red" : "blue"; }

//------------------------------------------------------------

void team_deathmatch::start(const char *plane, int color, int special, const char *location, int players_count)
{
    m_world.set_location(location);

    world::is_ally_handler fia = std::bind(&team_deathmatch::is_ally, this, std::placeholders::_1, std::placeholders::_2);
    m_world.set_ally_handler(fia);

    world::on_kill_handler fok = std::bind(&team_deathmatch::on_kill, this, std::placeholders::_1, std::placeholders::_2);
    m_world.set_on_kill_handler(fok);

    for (int t = 0; t < 2; ++t)
    {
        m_respawn_points[t].resize(players_count / 2);
        for (int i = 0; i < (int)m_respawn_points[t].size(); ++i)
        {
            auto &p = m_respawn_points[t][i];

            const int game_area_size = 8096;
            p.first.x = 0.25 * (-game_area_size/2 + (game_area_size / int(m_respawn_points[t].size())) * i);
            p.first.y = m_world.get_height(p.first.x, p.first.z) + 400.0f;
            p.first.z = game_area_size/2 * (t == 0 ? 1 : -1);

            p.second = quat(0.0, t == 0 ? nya_math::constants::pi : 0.0, 0.0);
        }
    }

    bool is_player = true;
    bool easter_edge = true;

    size_t plane_idx = 0;
    const auto planes = get_aircraft_ids({"fighter", "multirole"});
    assert(!planes.empty());

    m_bots.clear();
    for (int t = 0; t < 2; ++t)
    {
        for (int i = 0; i < players_count / 2; ++i)
        {
            plane_ptr p;
            if (is_player)
            {
                p = m_world.add_plane(plane, m_world.get_player_name(), color, is_player);
                is_player = false;
            }
            else
            {
                ai b;
                if (easter_edge)
                {
                    p = m_world.add_plane("f14d", "BOT", 3, is_player);
                    b.set_follow(m_world.get_player(), vec3(10.0, 0.0, -10.0));
                    easter_edge = false;
                }
                else
                {
                    const char *plane_name = planes[plane_idx = (plane_idx + 1) % planes.size()].c_str(); //ToDo
                    p = m_world.add_plane(plane_name, "BOT", 0, is_player);
                }

                b.set_plane(p);
                m_bots.push_back(b);
            }

            p->net_game_data.set("team", get_team(t));
            m_planes.push_back(p);

            auto rp = get_respawn_point(p->net_game_data.get<std::string>("team"));
            p->set_pos(rp.first);
            p->set_rot(rp.second);
        }
    }

    m_world.get_hud().set_team_score(0, 0);
}

//------------------------------------------------------------

void team_deathmatch::respawn(plane_ptr p)
{
    auto rp = get_respawn_point(p->net_game_data.get<std::string>("team"));
    p->set_pos(rp.first);
    p->set_rot(rp.second);
    p->reset_state();
}

//------------------------------------------------------------

void team_deathmatch::end()
{
    m_planes.clear();
}

//------------------------------------------------------------

team_deathmatch::respawn_point team_deathmatch::get_respawn_point(std::string team)
{
    const int t = get_team(team);

    if (m_respawn_points[t].empty())
        return respawn_point();

    return m_respawn_points[t][m_last_respawn[t] = (m_last_respawn[t] + 1) % m_respawn_points[t].size()];
}

//------------------------------------------------------------

bool team_deathmatch::is_ally(const plane_ptr &a, const plane_ptr &b)
{
    return a->net_game_data.get<std::string>("team") == b->net_game_data.get<std::string>("team");
}

//------------------------------------------------------------

void team_deathmatch::update_scores()
{
    deathmatch::update_scores();

    m_score[0] = m_score[1] = 0;

    for (int i = 0; i < m_world.get_planes_count(); ++i)
    {
        auto p = m_world.get_plane(i);
        m_score[get_team(p->net_game_data.get<std::string>("team"))] += p->net_game_data.get<int>("score");
    }

    m_world.get_hud().set_team_score(m_score[0], m_score[1]);
}

void team_deathmatch::on_kill(const plane_ptr &k, const plane_ptr &v)
{
    if (!v)
        return;

    if (k)
    {
        if (!is_ally(k, v))
            k->net_game_data.set("score", k->net_game_data.get<int>("score") + 1);
    }
    else
        v->net_game_data.set("score", v->net_game_data.get<int>("score") - 1);
}

//------------------------------------------------------------
}
