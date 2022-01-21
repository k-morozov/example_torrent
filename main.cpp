#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>

#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/write_resume_data.hpp>


namespace {

using clk = std::chrono::steady_clock;

// return the name of a torrent status enum
char const* state(lt::torrent_status::state_t s)
{
    switch(s) {
        case lt::torrent_status::checking_files: return "checking";
        case lt::torrent_status::downloading_metadata: return "dl metadata";
        case lt::torrent_status::downloading: return "downloading";
        case lt::torrent_status::finished: return "finished";
        case lt::torrent_status::seeding: return "seeding";
        case lt::torrent_status::checking_resume_data: return "checking resume";
        default: return "<>";
    }
}

} // anonymous namespace


int main() {
    std::string const url =
            "magnet:?xt=urn:btih:195d1c288a0ff7857006f7aa94e05b17d80fdbec&dn=JK%20Rowling%20-%20Harry%20Potter%20-%20Complete%20v3&tr=http%3A%2F%2Fbt2.t-ru.org%2Fann&tr=http%3A%2F%2Fretracker.local%2Fannounce";
//    "magnet:?xt=urn:btih:9fed9134b389a73c52e155dca6866c811308403c&dn=%D0%93%D0%B0%D1%80%D1%80%D0%B8%20%D0%9F%D0%BE%D1%82%D1%82%D0%B5%D1%80%20%D0%B8%20%D1%83%D0%B7%D0%BD%D0%B8%D0%BA%20%D0%90%D0%B7%D0%BA%D0%B0%D0%B1%D0%B0%D0%BD%D0%B0%20%282004%29&tr=http%3A%2F%2Fbt.t-ru.org%2Fann&tr=http%3A%2F%2Fretracker.local%2Fannounce";

    lt::settings_pack p;
    p.set_int(lt::settings_pack::alert_mask,
              lt::alert_category::status |
              lt::alert_category::storage |
              lt::alert_category::error);
    lt::session ses(p);
    clk::time_point last_save_resume = clk::now();

    std::ifstream ifs(".resume_file", std::ios_base::binary);
    ifs.unsetf(std::ios_base::skipws);
    std::vector<char> buf{std::istream_iterator<char>(ifs), std::istream_iterator<char>()};

    lt::add_torrent_params magnet = lt::parse_magnet_uri(url);
    if (!buf.empty())
    {
        lt::add_torrent_params atp = lt::read_resume_data(buf);
        if (atp.info_hashes == magnet.info_hashes)
            magnet = std::move(atp);
    }

    magnet.save_path = "."; // save in current dir
    ses.async_add_torrent(std::move(magnet));

    lt::torrent_handle h;
    bool done = false;

    for (;;)
    {
        std::vector<lt::alert*> alerts;
        ses.pop_alerts(&alerts);

        for (lt::alert const* a : alerts)
        {
            if (auto at = lt::alert_cast<lt::add_torrent_alert>(a))
                h = at->handle;
            if (lt::alert_cast<lt::torrent_finished_alert>(a))
            {
                h.save_resume_data(lt::torrent_handle::save_info_dict);
                done = true;
            }
            if (lt::alert_cast<lt::torrent_error_alert>(a))
            {
                std::cout << a->message() << std::endl;
                done = true;
                h.save_resume_data(lt::torrent_handle::save_info_dict);
            }
            if (auto rd = lt::alert_cast<lt::save_resume_data_alert>(a))
            {
                std::ofstream of(".resume_file", std::ios_base::binary);
                of.unsetf(std::ios_base::skipws);
                auto const b = write_resume_data_buf(rd->params);
                of.write(b.data(), int(b.size()));
                if (done)
                    goto done;
            }
            if (lt::alert_cast<lt::save_resume_data_failed_alert>(a))
            {
                if (done)
                    goto done;
            }
            if (auto st = lt::alert_cast<lt::state_update_alert>(a))
            {
                if (st->status.empty())
                    continue;

                lt::torrent_status const& status = st->status[0];
                std::cout << '\r' << state(status.state)
                    << " "
                    << (status.download_payload_rate / 1000) << " kB/s"
                    << (status.total_done / 1000) << " kB ("
                    << (status.progress_ppm / 10'000)  << "%) downloaded ("
                    << status.num_peers << " peers) \x1b[K";
                std::cout.flush();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        ses.post_torrent_updates();


        if (clk::now() - last_save_resume > std::chrono::seconds(5))
        {
            h.save_resume_data(lt::torrent_handle::save_info_dict);
            last_save_resume = clk::now();
        }
    }

    done:
        std::cout << "finished" << std::endl;
    return 0;
}
