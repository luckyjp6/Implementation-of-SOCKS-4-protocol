#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <unistd.h>
#include <cstring>
// #include <sys/wait.h>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#define max_length 1000000

using boost::asio::ip::tcp;
boost::asio::io_context io_context;

class client : public std::enable_shared_from_this<client>
{
public:
    client(tcp::socket s, tcp::socket c)
        :srv(std::move(s)), cli(std::move(c)) {}
    void start() {
        // std::cout << "in client" << std::endl;
        read_srv();
        read_cli();
    }

    void read_cli()
    {
        auto self(shared_from_this());
        memset(from_cli, '\0', max_length);    
        cli.async_read_some(boost::asio::buffer(from_cli, max_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    printf("from client: %s, length: %ld\n", from_cli, length); fflush(stdout);
                    write_srv(length);
                }
                else 
                {
                    std::cout << "bad cli socket" << std::endl;
                    exit(0);
                }
            });
    }
    void read_srv()
    {
        auto self(shared_from_this());
        memset(from_srv, '\0', max_length);    
        srv.async_read_some(boost::asio::buffer(from_srv, max_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    write_cli(length);
                }
                else 
                {
                    std::cout << "bad srv socket" << std::endl;
                    exit(0);
                }
            });
    }
    void write_cli(int length)
    {
        printf("\nto client: %d %ld\n%s\n", length, strlen(from_srv), from_srv); fflush(stdout);
        auto self(shared_from_this());
        boost::asio::async_write(cli, boost::asio::buffer(from_srv, length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    read_srv();
                }
                else 
                {
                    std::cout << "bad cli socket" << std::endl;
                    exit(0);
                }
            });
    }
    void write_srv(int length)
    {
        printf("\nto server: %d %ld\n%s\n", length, strlen(from_cli), from_cli); fflush(stdout);
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
                    std::cout << "bad srv socket" << std::endl;
                    exit(0);
                }
            });
    }
    tcp::socket srv, cli;
    char from_cli[max_length], from_srv[max_length];
};

class session : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket_) 
        :srv(io_context), cli(std::move(socket_)) {dst_port = 0; reply_state = 90;}
    
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
            // printf("%d ", rq_int[i]);
        }
        // printf("\n");
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
            std::size_t i = 8;
            while(i < length && rq[i]) i++;
            i++;
            char *start = rq+i;
            sprintf(domain_name, "%s", start);
            // printf("domain name: %s\n", domain_name);
        }
        fflush(stdout);  
    }
    
    void send_socks_reply()
    {
        char msg[10] = {0};
        msg[1] += reply_state;

        auto self(shared_from_this());
        boost::asio::async_write(cli, boost::asio::buffer(msg, 9),
            [this, self](boost::system::error_code ec, std::size_t /*length*/)
            {});
        if (reply_state != 90) end_client(false);
    }
    
    void connect_to_remote_server()
    {
        tcp::endpoint ep(boost::asio::ip::address::from_string(dst_ip), dst_port);
        // tcp::socket srv(io_context, ep.protocol());
        boost::system::error_code ec;
        srv.connect(ep, ec);
        if (ec) {reply_state = 92; return;}

        // if (cd == 1) std::make_shared<client>(std::move(srv), std::move(cli)) -> start();
        // else if (cd == 2) std::make_shared<FTP_client>(std::move(acceptor_), std::move(srv), std::move(cli)) -> start();
    }
    void get_server_ip()
    {
        // use domain name and port to get dst_ip
        // tcp::socket srv(io_context);
        tcp::resolver r(io_context);
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
                    printf("get request, length: %d\n", length);
                    my_parse_request(length);
                    
                    if (reply_state == 90)
                    {
                        // connect to the remote server
                        if (four_a) get_server_ip();
                        else connect_to_remote_server();
                    }
                
                    my_print_request();
                    send_socks_reply();  // terminate the session if reply_state != 90
                    
                    std::make_shared<client>(std::move(srv), std::move(cli)) -> start();
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
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) { do_accept(); }

private:
    void do_accept()
    {
        acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket_) {
            if (!ec) {
                int pid = fork();
                if (pid == 0) {
                    acceptor_.close();
                    std::make_shared<session>(std::move(socket_))->start();
                    io_context.run();
                    return;  
                }
                else {
                    socket_.close();
                    do_accept();
                }
            }
        });
    }

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
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}