#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <unistd.h>

//Third-party library: curl
#include <curl/curl.h>


//Third-party library: nlohmann json
#include "nlohmann/json.hpp"

//Third-party library: cardsorg/elo
#include "elo/elo.hpp"
//Third-party library: TaylorP/Glicko2
#include "glicko/rating.hpp"
//Third-party library: JesseBuesking/trueskill
#include "trueskill/trueskill.h"

//Third-party library: sciplot
#include "sciplot/sciplot.hpp"

using namespace std;
using namespace nlohmann;
using namespace sciplot;

//HTTP Response Callback
static size_t WriteCallback (void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

//HTTP GET Request
bool Get(string url, string *output_string) {
    CURL *Curl;
    Curl = curl_easy_init();
    if (Curl) {
        curl_easy_setopt(Curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(Curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(Curl, CURLOPT_WRITEDATA, output_string);
        CURLcode response = curl_easy_perform(Curl);
        curl_easy_cleanup(Curl);
        if(response == CURLcode::CURLE_OK)
            return true;
    }
    return false;
}

class Player;
class Match;


class Match {
public:
    Match () = default;

    void Import(string match_id) {
        id = std::move(match_id);
import_match:
        if(Get("https://arena.5eplay.com/data/match/" + id, &scoreboard_html)) {
            int score1_strstart = scoreboard_html.find("<p class=\"score mt10 fs60\">") + sizeof("<p class=\"score mt10 fs60\">") - 1;
            team_a_score = stoi(scoreboard_html.substr(score1_strstart, 2));

            int score2_strstart = scoreboard_html.find("<p class=\"score fs60\">") + sizeof("<p class=\"score fs60\">") - 1;
            team_b_score = stoi(scoreboard_html.substr(score2_strstart, 2));

            bool team_a_won = (team_a_score > team_b_score);
            match_draw = (team_a_score == team_b_score);

            string scoreboard = scoreboard_html;
            for(int l=0; l<5; l++) {
                int player_url_start = scoreboard.find("https://arena.5eplay.com/data/player/");
                int player_url_end = scoreboard.find("\"", player_url_start);

                if(player_id == scoreboard.substr(player_url_start + 37, player_url_end - player_url_start - 37)) {
                    player_team_a = true;
                }

                scoreboard = scoreboard.substr(player_url_end);
            }

            for(int l=0; l<5; l++) {
                int player_url_start = scoreboard.find("https://arena.5eplay.com/data/player/");
                int player_url_end = scoreboard.find("\"", player_url_start);

                if(player_id == scoreboard.substr(player_url_start + 37, player_url_end - player_url_start - 37)) {
                    player_team_a = false;
                }

                scoreboard = scoreboard.substr(player_url_end);
            }

            player_won = (player_team_a && team_a_won) || (!player_team_a && !team_a_won);

            cout << "   " << "Match " << "https://arena.5eplay.com/data/match/" << id << ": Score (" << team_a_score << "-" << team_b_score << ") " << (match_draw ? "match draw." : (player_won ? "player won." : "player lost.")) << endl;

        } else {
            cout << "   " << "Failed to import match " << id << ", retrying" <<endl;
            sleep(1);
            goto import_match;
        }

    }

    string id;
    string player_id;
    string scoreboard_html;
    bool player_won;
    bool player_team_a;
    bool match_draw;
    int team_a_score;
    int team_b_score;
};

class Player {
public:

    Player () = default;

    explicit Player(string player_id) {
        id = std::move(player_id);
    }
    string id;

    int imported_matches = 0;
    Match match_records[30];

    void ImportMatches() {
        if (id.empty()) return;
        cout << "   " << id << "'s Matches:" << endl;
        for (int p = 1; p<=3; p++) {
            get_matches:
            string response;
            bool http_success = Get("https://arena.5eplay.com/api/data/match_list/" + id + "?page=" + to_string(p) + "&yyyy=2022&match_type=", &response);

            json match_metadata_list = json::parse(response);

            if(!match_metadata_list["success"] || !http_success) {
                cout << "   " << "Failed to get player " + id + "'s matches!" << endl;
                continue;
            }
            for (auto match_metadata = match_metadata_list["data"].begin(); match_metadata != match_metadata_list["data"].end(); match_metadata++) {
                match_records[imported_matches].player_id = id;
                match_records[imported_matches].Import(match_metadata.value()["match_code"]);
                imported_matches++;
            }

        }

    }
};

class Simulation_Set {
public:
    Player team1[5];
    Player team2[5];
    Match checkpoint;
};

constexpr int simulation_sets = 20;

int main() {


    Simulation_Set sets[simulation_sets];

    bool use_existing = false;
    if(!use_existing) {

        string leaderboard_page;
        if (Get("https://arena.5eplay.com/data", &leaderboard_page)) {

            Player leaderboard_players[100];

            //Get the links to players on the leaderboard
            cout << "Leaderboards" << endl << "-----------------------------------" << endl;

            for (auto &i: leaderboard_players) {
                int player_url_start = leaderboard_page.find("https://arena.5eplay.com/data/player/");
                int player_url_end = leaderboard_page.find("\"", player_url_start);

                i.id = leaderboard_page.substr(player_url_start + 37, player_url_end - player_url_start - 37);
                cout << i.id << endl;

                leaderboard_page = leaderboard_page.substr(player_url_end);
            }


            cout << "-----------------------------------" << endl;
            cout << "Importing matches:" << endl;
            cout << "-----------------------------------" << endl;

            //We're sampling 10 players right???
            for (int i = 0; i < simulation_sets; i++) {
                //Log the information
                cout << "Sample Set " << i + 1 << endl;

                //First off, getting the matches of the leaderboard player
                leaderboard_players[i].ImportMatches();

                //After importing the matches of the first player, we'll trace down the players in the checkpoint matches
                Match checkpoint = leaderboard_players[i].match_records[0];
                sets[i].checkpoint = checkpoint;
                string checkpoint_scoreboard = checkpoint.scoreboard_html;
                //checkpoint match team 1
                for (int l = 0; l < 5; l++) {
                    int player_url_start = checkpoint_scoreboard.find("https://arena.5eplay.com/data/player/");
                    int player_url_end = checkpoint_scoreboard.find("\"", player_url_start);

                    sets[i].team1[l].id = checkpoint_scoreboard.substr(player_url_start + 37,
                                                                      player_url_end - player_url_start - 37);

                    //Import players matches, just copy class instance for the leaderboard player
                    if (sets[i].team1[l].id == leaderboard_players[i].id) sets[i].team1[l] = leaderboard_players[i];
                    else sets[i].team1[l].ImportMatches();

                    cout << endl;

                    checkpoint_scoreboard = checkpoint_scoreboard.substr(player_url_end);
                }
                //checkpoint match team 2
                for (int l = 0; l < 5; l++) {
                    int player_url_start = checkpoint_scoreboard.find("https://arena.5eplay.com/data/player/");
                    int player_url_end = checkpoint_scoreboard.find("\"", player_url_start);

                    sets[i].team2[l].id = checkpoint_scoreboard.substr(player_url_start + 37,
                                                                      player_url_end - player_url_start - 37);

                    if (sets[i].team2[l].id == leaderboard_players[i].id) sets[i].team2[l] = leaderboard_players[i];
                    else sets[i].team2[l].ImportMatches();

                    cout << endl;

                    checkpoint_scoreboard = checkpoint_scoreboard.substr(player_url_end);
                }

            }


        } else {
            cout << "Failed to fetch the leaderboard" << endl;
            return 1;
        }

    }

    //-------------------------------------------------------------------------

    //Rating process:
    int Elo_hits[3] = {0,0,0};
    int Glicko_hits[3] = {0,0,0};
    int TrueSkill_hits[3] = {0,0,0};
    vector<double> Elo_deviation[3];
    vector<double> Glicko_deviation[3];
    vector<double> TrueSkill_deviation[3];

    string group_names[3] = {"challenged", "standard", "calibrated"};
    int group_sets[3] = {5, 10, 20};

    for(int group = 0; group < 3; group++){
        cout << "Group " << group+1 << ": " << group_names[group] << " group" << endl;

        //Elo
        Plot Elo_plot;
        vector<int> Elo_plotx;
        vector<double> Elo_ploty;
        for (int set = 0; set < simulation_sets; set++) {

            Elo::Configuration elo_config(32);


            cout << "Rating players for set " + to_string(set+1) + " using Elo..." << endl;
            //Rate all players on team 1
            vector<Elo::Player> team1_player_elo;
            for (int team1_p = 0; team1_p < 5; team1_p++) {
                bool player_record = true;
                Player* player = &sets[set].team1[team1_p];
                Elo::Player player_elo(1500, elo_config);
                for (int rating_matches = group_sets[group]; rating_matches > 0; rating_matches--) {
                    if(player->imported_matches < rating_matches) {
                        player_record = false;
                        continue;
                    }
                    Match rating_match = player->match_records[rating_matches];
                    double rating_match_outcome = 0.5;
                    if(!rating_match.match_draw) {
                        if(rating_match.player_won)
                            rating_match_outcome = 1;
                        else
                            rating_match_outcome = 0;
                    }
                    else
                        rating_match_outcome = 0.5;
                    Elo::Player rating_opponent(player_elo.rating, elo_config);
                    Elo::Match rating_match_elo(player_elo, rating_opponent, rating_match_outcome);
                    rating_match_elo.apply();
                    if(player_record) {
                    Elo_plotx.push_back(1 + group_sets[group] - rating_matches);
                    Elo_ploty.push_back(player_elo.rating);
                    }
                }
                team1_player_elo.push_back(player_elo);
            }

            //Rate all players on team 2
            vector<Elo::Player> team2_player_elo;
            for (int team2_p = 0; team2_p < 5; team2_p++) {
                bool player_record = true;
                Player *player = &sets[set].team2[team2_p];
                Elo::Player player_elo(1500, elo_config);
                for (int rating_matches = group_sets[group]; rating_matches > 0; rating_matches--) {
                    if(player->imported_matches < rating_matches) {
                        player_record = false;
                        continue;
                    }
                    Match rating_match = player->match_records[rating_matches];
                    double rating_match_outcome = 0.5;
                    if (!rating_match.match_draw) {
                        if (rating_match.player_won)
                            rating_match_outcome = 1;
                        else
                            rating_match_outcome = 0.5;
                    }
                    Elo::Player rating_opponent(player_elo.rating, elo_config);
                    Elo::Match rating_match_elo(player_elo, rating_opponent, rating_match_outcome);
                    rating_match_elo.apply();
                    if(player_record){
                    Elo_plotx.push_back(1 + group_sets[group] - rating_matches);
                    Elo_ploty.push_back(player_elo.rating);
                    }
                }
                team2_player_elo.push_back(player_elo);
            }

            //Calculate average rating for both teams
            double team1_elo_avg = 0;
            double team2_elo_avg = 0;

            cout << "Team 1 Elo ratings: ";
            for (auto &player_elo : team1_player_elo) {
                team1_elo_avg += player_elo.rating;
                cout << player_elo.rating << " | ";
            }
            cout << endl;

            team1_elo_avg /= 5;
            cout << "Team 1 Elo average rating: " << team1_elo_avg << endl;

            cout << "Team 2 Elo ratings: ";
            for (auto &player_elo : team2_player_elo) {
                team2_elo_avg += player_elo.rating;
                cout << player_elo.rating << " | ";
            }
            cout << endl;

            team2_elo_avg /= 5;
            cout << "Team 2 Elo average rating: " << team2_elo_avg << endl;

            //Using the average rating to predict the outcome
            Match checkpoint = sets[set].checkpoint;
            Elo::Player team1_elo(team1_elo_avg, elo_config);
            Elo::Player team2_elo(team2_elo_avg, elo_config);
            Elo::Match match(team1_elo, team2_elo, checkpoint.player_won);

            double estimation_delta = match.estimate_outcome();
            if(!checkpoint.player_team_a) estimation_delta = 1.0 - estimation_delta;

            bool estimated_win = (estimation_delta >= 0.5);
            double deviation = abs(((checkpoint.player_team_a ? (double)checkpoint.team_a_score : (double)checkpoint.team_b_score) / (checkpoint.team_a_score + checkpoint.team_b_score)) * 1 - estimation_delta ) * 100;

            if((estimated_win && checkpoint.player_won) || (!estimated_win && !checkpoint.player_won)) Elo_hits[group]++;
            Elo_deviation[group].push_back(deviation);

            cout << "Predicted outcome: " << estimation_delta * 100 << "% chance of winning | Actual outcome: " << (checkpoint.match_draw ? "draw" : (checkpoint.player_won ? "player won" : "player lost")) << endl;
            cout << "Score difference (" << checkpoint.team_a_score << "-" << checkpoint.team_b_score << ") = " << abs(checkpoint.team_a_score - checkpoint.team_b_score) << " Percentage: " << ((checkpoint.player_team_a ? (double)checkpoint.team_a_score : (double)checkpoint.team_b_score) / (double)(checkpoint.team_a_score + checkpoint.team_b_score)) * 100 << "%" << endl;
            cout << "Prediction - difference deviation: " << deviation << "%" << endl;
            cout << endl;
        }
        Elo_plot.drawPoints(Elo_plotx, Elo_ploty).pointSize(1).pointType(1);
        Elo_plot.legend().hide();
        Elo_plot.xlabel("Rating Rounds");
        Elo_plot.ylabel("Rating");

        Elo_plot.save("Elo_" + group_names[group]+".pdf");

        //Glicko-2

        Plot Glicko_rating_plot;
        vector<int> Glicko_rating_plotx;
        vector<double> Glicko_rating_ploty;
        Plot Glicko_deviation_plot;
        vector<int> Glicko_deviation_plotx;
        vector<double> Glicko_deviation_ploty;
        Plot Glicko_volatility_plot;
        vector<int> Glicko_volatility_plotx;
        vector<double> Glicko_volatility_ploty;
        for (int set = 0; set < simulation_sets; set++) {
            cout << "Rating players for set " + to_string(set+1) + " using Glicko..." << endl;

            vector<Glicko::Rating> team1_glicko_rating;
            for (int team1_p = 0; team1_p < 5; team1_p++) {
                bool player_record = true;
                Player* player = &sets[set].team1[team1_p];
                Glicko::Rating player_glicko(1500, 350, 0.06);
                for (int rating_matches = group_sets[group]; rating_matches > 0; rating_matches--) {
                    if(player->imported_matches < rating_matches) {
                        player_record = false;
                        continue;
                    }
                    Match rating_match = player->match_records[rating_matches];
                    Glicko::Rating opponent_glicko(player_glicko.Rating1(), 350, 0.06);

                    vector<double> rating_match_outcomes;
                    vector<Glicko::Rating> opponents_glicko;
                    opponents_glicko.push_back(opponent_glicko);
                    rating_match_outcomes.push_back(rating_match.match_draw ? 0.5 : (double)rating_match.player_won);
                    player_glicko.Update(1, opponents_glicko.data(), rating_match_outcomes.data());
                    player_glicko.Apply();

                    if(player_record){
                        Glicko_rating_plotx.push_back(1+group_sets[group]-rating_matches);
                        Glicko_deviation_plotx.push_back(1+group_sets[group]-rating_matches);
                        Glicko_volatility_plotx.push_back(1+group_sets[group]-rating_matches);

                        Glicko_rating_ploty.push_back(player_glicko.Rating1());
                        Glicko_deviation_ploty.push_back(player_glicko.Deviation1());
                        Glicko_volatility_ploty.push_back(player_glicko.s);
                    }

                }

                team1_glicko_rating.push_back(player_glicko);
            }

            vector<Glicko::Rating> team2_glicko_rating;
            for (int team2_p = 0; team2_p < 5; team2_p++) {
                bool player_record = true;
                Player* player = &sets[set].team2[team2_p];
                Glicko::Rating player_glicko(1500, 350, 0.06);
                int actual_rating_matches = 0;
                for (int rating_matches = group_sets[group]; rating_matches > 0; rating_matches--) {
                    if(player->imported_matches < rating_matches) {
                        player_record = false;
                        continue;
                    }
                    actual_rating_matches++;
                    Match rating_match = player->match_records[rating_matches];
                    Glicko::Rating opponent_glicko(player_glicko.Rating1(), player_glicko.Deviation1(), 0.06);
                    
                    vector<double> rating_match_outcomes;
                    vector<Glicko::Rating> opponents_glicko;
                    
                    opponents_glicko.push_back(opponent_glicko);
                    rating_match_outcomes.push_back(rating_match.match_draw ? 0.5 : (double)rating_match.player_won);

                    player_glicko.Update(1, opponents_glicko.data(), rating_match_outcomes.data());
                    player_glicko.Apply();
                    if(player_record){
                        Glicko_rating_plotx.push_back(1+group_sets[group]-rating_matches);
                        Glicko_deviation_plotx.push_back(1+group_sets[group]-rating_matches);
                        Glicko_volatility_plotx.push_back(1+group_sets[group]-rating_matches);

                        Glicko_rating_ploty.push_back(player_glicko.Rating1());
                        Glicko_deviation_ploty.push_back(player_glicko.Deviation1());
                        Glicko_volatility_ploty.push_back(player_glicko.s);
                    }
                }
                team2_glicko_rating.push_back(player_glicko);
            }
            
            Glicko_rating_plot.drawPoints(Glicko_rating_plotx,Glicko_rating_ploty).pointSize(1).pointType(1);
            Glicko_deviation_plot.drawPoints(Glicko_deviation_plotx,Glicko_deviation_ploty).pointSize(1).pointType(1);
            Glicko_volatility_plot.drawPoints(Glicko_volatility_plotx,Glicko_volatility_ploty).pointSize(1).pointType(1);
            
            Glicko_rating_plot.xlabel("Rating Rounds");
            Glicko_deviation_plot.xlabel("Rating Rounds");
            Glicko_volatility_plot.xlabel("Rating Rounds");

            Glicko_rating_plot.ylabel("Rating");
            Glicko_deviation_plot.ylabel("Deviation");
            Glicko_volatility_plot.ylabel("Volatility");

            Glicko_rating_plot.legend().hide();
            Glicko_deviation_plot.legend().hide();
            Glicko_volatility_plot.legend().hide();
            
            Glicko_rating_plot.save("Glicko_rating_" + group_names[group]+".pdf");
            Glicko_deviation_plot.save("Glicko_deviation_" + group_names[group]+".pdf");
            Glicko_volatility_plot.save("Glicko_volatility_" + group_names[group]+".pdf");
            
            //Calculate average rating for both teams
            double team1_glicko_avg = 0;
            double team2_glicko_avg = 0;

            cout << "Team 1 Glicko ratings: ";
            for (auto &player_glicko : team1_glicko_rating) {
                team1_glicko_avg += player_glicko.Rating1();
                cout << player_glicko.Rating1() << " | ";
            }
            cout << endl;

            team1_glicko_avg /= 5;
            cout << "Team 1 Glicko average rating: " << team1_glicko_avg << endl;

            cout << "Team 2 Glicko ratings: ";
            for (auto &player_glicko : team2_glicko_rating) {
                team2_glicko_avg += player_glicko.Rating1();
                cout << player_glicko.Rating1() << " | ";
            }
            cout << endl;

            team2_glicko_avg /= 5;
            cout << "Team 2 Elo average rating: " << team2_glicko_avg << endl;

            //Using the average rating to predict the outcome
            Match checkpoint = sets[set].checkpoint;
            Glicko::Rating team1_glicko(team1_glicko_avg);
            Glicko::Rating team2_glicko(team2_glicko_avg);

            double estimation_delta = team2_glicko.E(team2_glicko.G(), team1_glicko);
            if(!checkpoint.player_team_a) estimation_delta = team1_glicko.E(team1_glicko.G(), team2_glicko);

            bool estimated_win = (estimation_delta >= 0.5);
            double deviation = abs(((checkpoint.player_team_a ? (double)checkpoint.team_a_score : (double)checkpoint.team_b_score) / (double)(checkpoint.team_a_score + checkpoint.team_b_score)) * 1 - estimation_delta ) * 100;

            if((estimated_win && checkpoint.player_won) || (!estimated_win && !checkpoint.player_won)) Glicko_hits[group]++;
            Glicko_deviation[group].push_back(deviation);

            cout << "Predicted outcome: " << estimation_delta * 100 << "% chance of winning | Actual outcome: " << (checkpoint.match_draw ? "draw" : (checkpoint.player_won ? "player won" : "player lost")) << endl;
            cout << "Score difference (" << checkpoint.team_a_score << "-" << checkpoint.team_b_score << ") = " << abs(checkpoint.team_a_score - checkpoint.team_b_score) << " Percentage: " << ((checkpoint.player_team_a ? (double)checkpoint.team_a_score : (double)checkpoint.team_b_score) / (double)(checkpoint.team_a_score + checkpoint.team_b_score)) * 100 << "%" << endl;
            cout << "Prediction - difference deviation: " << deviation << "%" << endl;
            cout << endl;
        }

        //TrueSkill
        Plot TrueSkill_rating_plot;
        vector<int> TrueSkill_rating_plotx;
        vector<double> TrueSkill_rating_ploty;
        Plot TrueSkill_deviation_plot;
        vector<int> TrueSkill_deviation_plotx;
        vector<double> TrueSkill_deviation_ploty;
        for (int set = 0; set < simulation_sets; set++) {
            cout << "Rating players for set " + to_string(set + 1) + " using TrueSkill..." << endl;
            vector<TrueSkill::Player*> team1_trueskill_rating;
            for (int team1_p = 0; team1_p < 5; team1_p++) {
                bool player_record = true;
                Player player = sets[set].team1[team1_p];
                auto player_trueskill = new TrueSkill::Player();
                player_trueskill->mu = 25.0;
                player_trueskill->sigma = 25.0 / 3.0;
                for (int rating_matches = group_sets[group]; rating_matches > 0; rating_matches--) {
                    if(player.imported_matches < rating_matches) {
                        player_record = false;
                        continue;
                    }
                    Match rating_match = player.match_records[rating_matches];
                    auto opponent_trueskill = new TrueSkill::Player();
                    opponent_trueskill->mu = player_trueskill->mu;
                    opponent_trueskill->sigma = player_trueskill->sigma;
                    vector<TrueSkill::Player*>match_trueskill;
                    if (rating_match.player_won) {
                        player_trueskill->rank = 1;
                        opponent_trueskill->rank = 2;
                    } else if (rating_match.match_draw) {
                        player_trueskill->rank = 1;
                        opponent_trueskill->rank = 1;
                    } else {
                        opponent_trueskill->rank = 1;
                        player_trueskill->rank = 2;
                    }
                    match_trueskill.push_back(player_trueskill);
                    match_trueskill.push_back(opponent_trueskill);
                    TrueSkill::adjust_players(match_trueskill);

                    if(player_record){
                    TrueSkill_rating_plotx.push_back(1+group_sets[group]-rating_matches);
                    TrueSkill_deviation_plotx.push_back(1+group_sets[group]-rating_matches);

                    TrueSkill_rating_ploty.push_back(player_trueskill->mu);
                    TrueSkill_deviation_ploty.push_back(player_trueskill->sigma);
                    }
                }

                team1_trueskill_rating.push_back(player_trueskill);
            }

            vector<TrueSkill::Player*> team2_trueskill_rating;
            for (int team2_p = 0; team2_p < 5; team2_p++) {
                bool player_record = true;
                Player player = sets[set].team2[team2_p];
                auto player_trueskill = new TrueSkill::Player();
                player_trueskill->mu = 25.0;
                player_trueskill->sigma = 25.0 / 3.0;
                for (int rating_matches = group_sets[group]; rating_matches > 0; rating_matches--) {
                    if(player.imported_matches < rating_matches) {
                        player_record = false;
                        continue;
                    }
                    Match rating_match = player.match_records[rating_matches];
                    auto opponent_trueskill = new TrueSkill::Player();
                    opponent_trueskill->mu = player_trueskill->mu;
                    opponent_trueskill->sigma = player_trueskill->sigma;
                    vector<TrueSkill::Player*>match_trueskill;
                    if (rating_match.player_won) {
                        player_trueskill->rank = 1;
                        opponent_trueskill->rank = 2;
                    } else if (rating_match.match_draw) {
                        player_trueskill->rank = 1;
                        opponent_trueskill->rank = 1;
                    } else {
                        opponent_trueskill->rank = 1;
                        player_trueskill->rank = 2;
                    }
                    match_trueskill.push_back(player_trueskill);
                    match_trueskill.push_back(opponent_trueskill);
                    TrueSkill::adjust_players(match_trueskill);
                    if(player_record){
                    TrueSkill_rating_plotx.push_back(1+group_sets[group]-rating_matches);
                    TrueSkill_deviation_plotx.push_back(1+group_sets[group]-rating_matches);

                    TrueSkill_rating_ploty.push_back(player_trueskill->mu);
                    TrueSkill_deviation_ploty.push_back(player_trueskill->sigma);
                    }
                }
                team2_trueskill_rating.push_back(player_trueskill);
                
            }

            TrueSkill_rating_plot.drawPoints(TrueSkill_rating_plotx,TrueSkill_rating_ploty).pointSize(1).pointType(1);
            TrueSkill_deviation_plot.drawPoints(TrueSkill_deviation_plotx,TrueSkill_deviation_ploty).pointSize(1).pointType(1);

            TrueSkill_rating_plot.xlabel("Rating Rounds");
            TrueSkill_deviation_plot.xlabel("Rating Rounds");

            TrueSkill_rating_plot.ylabel("Rating");
            TrueSkill_deviation_plot.ylabel("Deviation");

            TrueSkill_rating_plot.legend().hide();
            TrueSkill_deviation_plot.legend().hide();

            TrueSkill_rating_plot.save("TrueSkill_rating_" + group_names[group]+".pdf");
            TrueSkill_deviation_plot.save("TrueSkill_deviation_" + group_names[group]+".pdf");

            double team1_trueskill_avg = 0;
            double team2_trueskill_avg = 0;

            cout << "Team 1 TrueSkill ratings: ";
            for (auto &player_trueskill : team1_trueskill_rating) {
                team1_trueskill_avg += player_trueskill->mu;
                cout << player_trueskill->mu << " | ";
            }
            cout << endl;
            team1_trueskill_avg /= 5;
            cout << "Team 1 average TrueSkill rating: " << team1_trueskill_avg << endl;

            cout << "Team 2 TrueSkill ratings: ";
            for (auto &player_trueskill : team2_trueskill_rating) {
                team2_trueskill_avg += player_trueskill->mu;
                cout << player_trueskill->mu << " | ";
            }
            cout << endl;
            team2_trueskill_avg /= 5;
            cout << "Team 2 average TrueSkill rating: " << team2_trueskill_avg << endl;

            auto team1_trueskill = new TrueSkill::Player();
            auto team2_trueskill = new TrueSkill::Player();
            team1_trueskill->mu = team1_trueskill_avg;
            team1_trueskill->sigma = team1_trueskill_avg / 3.0;
            team2_trueskill->mu = team2_trueskill_avg;
            team2_trueskill->sigma = team2_trueskill_avg / 3.0;

            vector<TrueSkill::Player*> checkpoint_trueskill;
            checkpoint_trueskill.push_back(team1_trueskill);
            checkpoint_trueskill.push_back(team2_trueskill);


            //Using the average rating to predict the outcome
            Match checkpoint = sets[set].checkpoint;
            double estimation_delta = team1_trueskill_avg / (team1_trueskill_avg + team2_trueskill_avg);
            if(!checkpoint.player_team_a) estimation_delta = 1 - estimation_delta;
            bool estimated_win = (estimation_delta >= 0.5);
            double deviation = abs(((checkpoint.player_team_a ? (double)checkpoint.team_a_score : (double)checkpoint.team_b_score) / (double)(checkpoint.team_a_score + checkpoint.team_b_score)) * 1 - estimation_delta ) * 100;

            if((estimated_win && checkpoint.player_won) || (!estimated_win && !checkpoint.player_won)) TrueSkill_hits[group]++;
            TrueSkill_deviation[group].push_back(deviation);

            cout << "Predicted outcome: " << estimation_delta * 100 << "% chance of winning | Actual outcome: " << (checkpoint.match_draw ? "draw" : (checkpoint.player_won ? "player won" : "player lost")) << endl;
            cout << "Score difference (" << checkpoint.team_a_score << "-" << checkpoint.team_b_score << ") = " << abs(checkpoint.team_a_score - checkpoint.team_b_score) << " Percentage: " << ((checkpoint.player_team_a ? (double)checkpoint.team_a_score : (double)checkpoint.team_b_score) / (double)(checkpoint.team_a_score + checkpoint.team_b_score)) * 100 << "%" << endl;
            cout << "Prediction - difference deviation: " << deviation << "%" << endl;
            cout << endl;
        }
    }
    cout << endl << "---------------------------------------------" << endl;
    cout << "Rating Quality Summary: " << endl;

    for (int group = 0; group < 3; group++) {
        cout << endl << "Results of " << group_names[group] << " run" << endl;

        cout << "   " << "Elo percentage of correct match estimation: " << ((double)Elo_hits[group] / (double)simulation_sets) * 100 << "% (" << Elo_hits[group] << "/" << simulation_sets << ")" << endl;
        cout << "   " << "Glicko percentage of correct match estimation: " << ((double)Glicko_hits[group] / (double)simulation_sets) * 100 << "% (" << Glicko_hits[group] << "/" << simulation_sets << ")" << endl;
        cout << "   " << "TrueSkill percentage of correct match estimation: " << ((double)TrueSkill_hits[group] / (double)simulation_sets) * 100 << "% (" << TrueSkill_hits[group] << "/" << simulation_sets << ")" << endl;

        cout << endl;

        double Elo_deviation_avg = 0;
        for (auto deviation : Elo_deviation[group]) {
            Elo_deviation_avg += deviation;
        }
        Elo_deviation_avg /= simulation_sets;

        double Glicko_deviation_avg = 0;
        for (auto deviation : Glicko_deviation[group]) {
            Glicko_deviation_avg += deviation;
        }
        Glicko_deviation_avg /= simulation_sets;

        double TrueSkill_deviation_avg = 0;
        for (auto deviation : TrueSkill_deviation[group]) {
            TrueSkill_deviation_avg += deviation;
        }
        TrueSkill_deviation_avg /= simulation_sets;

        cout << "   " << "Elo average deviation: " << Elo_deviation_avg << endl;
        cout << "   " << "Glicko average deviation: " << Glicko_deviation_avg << endl;
        cout << "   " << "TrueSkill average deviation: " << TrueSkill_deviation_avg << endl;
        cout << endl;
    }

    return 0;
}
