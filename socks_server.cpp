#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#define max_length 100000

using boost::asio::ip::tcp;
boost::asio::io_context io_c;

class client : public std::enable_shared_from_this<client>
{
public:
    client(tcp::socket s, tcp::socket c)
        :srv(std::move(s)), cli(std::move(c)) {}
    void start() {
        read_srv();
        read_cli();
    }
private:
    void read_cli()
    {
        auto self(shared_from_this());
        memset(from_cli, 0, max_length);
        cli.async_read_some(boost::asio::buffer(from_cli, max_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    // printf("from client: %s, length: %ld\n", from_cli, length); fflush(stdout);
                    write_srv(length);
                }
                else 
                {
                    // std::cout << "bad cli socket" << std::endl;
                    exit(0);
                }
            });
    }
    void read_srv()
    {
        auto self(shared_from_this());
        memset(from_srv, 0, max_length);
        srv.async_read_some(boost::asio::buffer(from_srv, max_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    write_cli(length);
                }
                else 
                {
                    // std::cout << "bad srv socket" << std::endl;
                    exit(0);
                }
            });
    }
    void write_cli(int length)
    {
        // printf("\nto client: %d %ld\n", length, strlen(from_srv)); fflush(stdout);
        if (length <= 0) return;
        auto self(shared_from_this());
        boost::asio::async_write(cli, boost::asio::buffer(from_srv, length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    // printf("actual send: %ld\n", length);fflush(stdout);
                    read_srv();
                }
                else 
                {
                    // std::cout << "bad cli socket" << std::endl;
                    exit(0);
                }
            });
    }
    void write_srv(int length)
    {
        // printf("\nto server: %d %ld\n%s\n", length, strlen(from_cli), from_cli); fflush(stdout);
        auto self(shared_from_this());
        boost::asio::async_write(srv, boost::asio::buffer(from_cli, length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    read_cli();
                }
                else 
                {
                    // std::cout << "bad srv socket" << std::endl;
                    exit(0);
                }
            });
    }
    tcp::socket srv, cli;
    char from_cli[max_length], from_srv[max_length];
};

class FTP_client : public std::enable_shared_from_this<FTP_client>
{
public:
    FTP_client(short port)
        :skt_A(io_c), skt_B(io_c), acceptor_(io_c, tcp::endpoint(tcp::v4(), port)) {}
    void start() {
        get_num = 0;
        do_accept();
    }
private:
    void do_accept()
    {
        acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket_) {
            if (!ec) {
                if (get_num == 0) skt_A = std::move(socket_);
                else if (get_num == 1) skt_B = std::move(socket_);
                get_num++;
                if (get_num == 2)
                {
                    read_A();
                    read_B();
                }
                else do_accept();
            }
        });
    }
    void read_A()
    {
        auto self(shared_from_this());
        memset(from_A, '\0', max_length);    
        skt_A.async_read_some(boost::asio::buffer(from_A, max_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    // printf("from A: %s, length: %ld\n", from_A, length); fflush(stdout);
                    write_B(length);
                }
                else 
                {
                    std::cout << "bad A socket" << std::endl;
                    exit(0);
                }
            });
    }
    void read_B()
    {
        auto self(shared_from_this());
        memset(from_B, '\0', max_length);    
        skt_B.async_read_some(boost::asio::buffer(from_B, max_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    write_A(length);
                }
                else 
                {
                    std::cout << "bad B socket" << std::endl;
                    exit(0);
                }
            });
    }
    void write_A(int length)
    {
        // printf("\nto client: %d %ld\n%s\n", length, strlen(from_srv), from_srv); fflush(stdout);
        auto self(shared_from_this());
        boost::asio::async_write(skt_A, boost::asio::buffer(from_B, length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    read_B();
                }
                else 
                {
                    // std::cout << "bad cli socket" << std::endl;
                    exit(0);
                }
            });
    }
    void write_B(int length)
    {
        // printf("\nto server: %d %ld\n%s\n", length, strlen(from_cli), from_cli); fflush(stdout);
        auto self(shared_from_this());
        boost::asio::async_write(skt_B, boost::asio::buffer(from_A, length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    read_A();
                }
                else 
                {
                    // std::cout << "bad srv socket" << std::endl;
                    exit(0);
                }
            });
    }
    
    int get_num;
    tcp::socket skt_A, skt_B;
    tcp::acceptor acceptor_;
    char from_A[max_length], from_B[max_length];
};

class session : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket_, short port) 
        :srv(io_c), cli(std::move(socket_)) {dst_port = 0; reply_state = 90; available_port = port;}
    
    void start() { read_req(); }

private:
    void end_client(bool ok)
    {
        cli.close();

        if (ok) srv.close();

        exit(0);
    }
    void my_print_request()
    {
        // <S_IP>: source ip
        printf("<S_IP>: %s\n", cli.remote_endpoint().address().to_string().data()); 
        // <S_PORT>: source prot
        printf("<S_PORT>: %d\n", cli.remote_endpoint().port()); 
        // <D_IP>: destination ip
        printf("<D_IP>: %s\n", dst_ip); 
        // <D_PORT>: destination port
        printf("<D_PORT>: %d\n", dst_port); 
        
        // <COMMAND>: CONNECT or BIND
        if (cd == 1) printf("CONNECT\n"); 
        else if (cd == 2) printf("BIND\n");
         
        // <Reply>: Accept or Reject
        if (reply_state == 90) printf("Accept\n");
        else printf("Reject\n");

        printf("\n");
        
        fflush(stdout);
        return;
    }
    void my_parse_request(int length)
    {
        // printf("request length: %d\n", length);
        int i = 0;
        int rq_int[8] = {0};
        for (i = 0; i < 8 ; i++)
        {
            rq_int[i] = (int)((unsigned char)(rq[i]));
            printf("%d ", rq_int[i]);
        }
        printf("\n");
        if (i < 8) {reply_state = 91; return; }
        vn = rq_int[0];
        cd = rq_int[1];
        if (vn != 4) {reply_state = 91; return;}
        if (cd != 1 && cd != 2) {reply_state = 91; return;}
        dst_port = rq_int[2]*256 + rq_int[3];

        if ((rq_int[4] == 0) && (rq_int[5] == 0) && (rq_int[6] == 0) && rq_int[7] != 0) four_a = true;
        sprintf(dst_ip, "%d.%d.%d.%d", rq_int[4], rq_int[5], rq_int[6], rq_int[7]);

        if (four_a)
        {
            int i = 8;
            while(i < length && rq[i]) i++;
            i++;
            char *start = rq+i;
            sprintf(domain_name, "%s", start);
            // printf("domain name: %s\n", domain_name);
        }
        fflush(stdout);  
    }
    void my_check_request()
    {
        int conf = open("socks.conf", O_RDONLY);
        char line[10000];
        int len = read(conf, line, 10000);

        if (len <= 0)
        {
            reply_state = 91;
            return;
        }

        int ip_slice[4];
        sscanf(dst_ip, "%d.%d.%d.%d", &ip_slice[0], &ip_slice[1], &ip_slice[2], &ip_slice[3]);

        char *now = line;
        char *remain = line;
        while(true)
        {
            now = strtok_r(remain, "\n", &remain);
            if (now == NULL) break;
            
            char command;
            char ip_part[20];
            sscanf(now, "permit %c %s", &command, ip_part);
            if (cd == 1 && command != 'c') continue;
            if (cd == 2 && command != 'b') continue;

            char *ii = ip_part;
            bool met = true;
            for (int i = 0; i < 4; i++) 
            {
                char *n = strtok_r(ii, ".", &ii);
                if (n == NULL) break;
                if (strcmp("*", n) == 0) continue;
                if (atoi(n) != ip_slice[i])
                {
                    met = false;
                    break;
                }
            }
            if (met) return;
        }

        reply_state = 91;
        return;
    }

    void send_socks_reply()
    {
        char msg[10] = {0};
        msg[1] += reply_state;

        auto self(shared_from_this());
        boost::asio::async_write(cli, boost::asio::buffer(msg, 8),
            [this, self](boost::system::error_code ec, std::size_t length){});
            
        if (reply_state != 90) end_client(false);
    }
    void send_bind_reply()
    {
        char msg[10] = {0};
        msg[1] += reply_state;

        msg[2] += available_port/256;
        msg[3] += available_port%256;

        auto self(shared_from_this());
        boost::asio::async_write(cli, boost::asio::buffer(msg, 8),
            [this, self](boost::system::error_code ec, std::size_t length)
            {});
    }

    void connect_to_remote_server()
    {
        printf("start connecting...\n");
        tcp::endpoint ep(boost::asio::ip::address::from_string(dst_ip), dst_port);
        // tcp::socket srv(io_c, ep.protocol());
        boost::system::error_code ec;
        srv.connect(ep, ec);
        
        if (ec) {reply_state = 92; return;}
        else printf("connect successfully\n");
        // if (cd == 1) std::make_shared<client>(std::move(srv), std::move(cli)) -> start();
        // else if (cd == 2) std::make_shared<FTP_client>(std::move(acceptor_), std::move(srv), std::move(cli)) -> start();
    }
    void get_server_ip()
    {
        // use domain name and port to get dst_ip
        // tcp::socket srv(io_c);
        tcp::resolver r(io_c);
        char my_port[10];
        memset(my_port, '\0', 10);
        sprintf(my_port, "%d", dst_port);
        tcp::resolver::query q(domain_name, my_port);
        auto iter = r.resolve(q);
        decltype(iter) end;
        boost::system::error_code ec = boost::asio::error::host_not_found;
        for (; ec && iter != end; ++iter)
        {
            srv.connect(*iter, ec);
            if (ec) srv.close();
            // else break;
        }
        if (ec) {reply_state = 92; return;}
        strcpy(dst_ip, srv.remote_endpoint().address().to_string().data());
        // printf("ip: %s, port: %d\n", srv.remote_endpoint().address().to_string().data(), srv.remote_endpoint().port());
    }
    
    void read_req()
    {
        auto self(shared_from_this());

        memset(rq, '\0', max_length);
        cli.async_read_some(boost::asio::buffer(rq, max_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    // printf("get request, length: %ld\n", length);
                    my_parse_request(length);

                    my_check_request();
                    if (reply_state != 90) 
                    {
                        my_print_request();
                        send_socks_reply();// terminate the process if reply_state != 90
                    }
                    
                    if (cd == 1)
                    {
                        // connect to the remote server
                        io_c.notify_fork(boost::asio::io_context::fork_prepare);
                        int pid = fork();
                        if (pid < 0)
                        {
                            printf("failed to fork\n");
                            exit(0);
                        }
                        else if (pid == 0)
                        {
                            io_c.notify_fork(boost::asio::io_context::fork_child);
                            if (four_a) get_server_ip();
                            else connect_to_remote_server();
                            my_print_request();
                            send_socks_reply();// terminate the process if reply_state != 90
                            std::make_shared<client>(std::move(srv), std::move(cli)) -> start();
                            return;
                        }
                        else 
                        {
                            io_c.notify_fork(boost::asio::io_context::fork_parent);
                            srv.close();
                            cli.close();
                            return;
                        }
                    }
                    // else if (cd == 2)
                    // {
                    //     io_c.notify_fork(boost::asio::io_context::fork_prepare);
                    //     int pid = fork();
                    //     if (pid < 0)
                    //     {
                    //         printf("failed to fork\n");
                    //         do_accept();
                    //         exit(0);
                    //     }
                    //     else if (pid == 0)
                    //     {
                    //         io_c.notify_fork(boost::asio::io_context::fork_child);
                            
                    //         my_print_request();
                    //         send_bind_reply();// terminate the process if reply_state != 90
                    //         return;
                    //     }
                    //     else io_c.notify_fork(boost::asio::io_context::fork_parent);
                    // }
                    // if (cd == 2)
                    // {
                    //     int pid = fork();
                    //     if (pid == 0)
                    //     {
                    //         send_bind_reply();
                    //         srv.close();
                    //         cli.close();
                    //         std::make_shared<FTP_client>(available_port) -> start();
                    //     }
                    // }
                }
            });
        return;
    }
    void do_write(std::string msg)
    {
        auto self(shared_from_this());
        boost::asio::async_write(cli, boost::asio::buffer(msg.data(), msg.size()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/)
            {});
    }

    short available_port;
    tcp::socket srv, cli;
    bool four_a = false;
    int reply_state;
    char rq[max_length];
    int vn, cd, userid, dst_port;
    char dst_ip[20], domain_name[max_length];
};


class server
{
public:
    server(short port)
        : acceptor_(io_c, tcp::endpoint(tcp::v4(), port)) { do_accept(); available_port = port;}

private:
    void do_accept()
    {
        acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket_) {
            if (!ec) {
                io_c.notify_fork(boost::asio::io_context::fork_prepare);
                int pid = fork();
                if (pid < 0)
                {
                    printf("failed to fork\n");
                    do_accept();
                    return;
                }
                if (pid == 0)
                {
                    io_c.notify_fork(boost::asio::io_context::fork_child);
                    acceptor_.close();
                    std::make_shared<session>(std::move(socket_), available_port)->start();
                }
                if (pid > 0)
                {
                    io_c.notify_fork(boost::asio::io_context::fork_parent);
                    available_port++;
                    socket_.close();
                    do_accept();
                }                
            }
        });
    }

    short available_port;
    tcp::acceptor acceptor_;
};

// void sig_chld(int signo)
// {
// 	int	stat;

// 	while (waitpid(-1, &stat, WNOHANG) > 0);

// 	return;
// }

int main(int argc, char **argv)
{
    // signal(SIGCHLD, sig_chld);
    if (argc != 2)
    {
        std::cerr << "Usage: socks_server <port>\n";
        return 1;
    }
    try
    {
        server s(std::atoi(argv[1]));
        io_c.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}