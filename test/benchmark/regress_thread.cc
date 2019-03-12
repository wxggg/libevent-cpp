#include <thread_pool.hh>

#include <iostream>

using namespace std;
using namespace eve;


void zero()
{
    std::cout << "hello from " << ", function\n";
}

void first(int id)
{
    for(int i = 0; i<id; i++)
        if(i % 13731 == 0) cout<<"X"<<endl;
    std::cout << "hello from " << id << ", function\n";
}

void aga(int id, int par)
{
    std::cout << "hello from " << id << ", function with parameter " << par << '\n';
}
void mmm(int id, const std::string &s)
{
    std::cout << "mmm function " << id << ' ' << s << '\n';
}


struct Third
{
    Third(int v)
    {
        this->v = v;
        std::cout << "Third ctor " << this->v << '\n';
    }
    Third(Third &&c)
    {
        this->v = c.v;
        std::cout << "Third move ctor\n";
    }
    Third(const Third &c)
    {
        this->v = c.v;
        std::cout << "Third copy ctor\n";
    }
    ~Third() { std::cout << "Third dtor\n"; }
    int v;
};
void ugu(int id, Third &t)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    std::cout << "hello from " << id << ", function with parameter Third " << t.v << '\n';
}

int main(int argc, char const *argv[])
{
    thread_pool pool(8);
    for(int i=0; i<10000;i++)
    {
        pool.push(first, i);
    }
    // pool.push(std::ref(zero));

    // pool.push(first, 1);
    // pool.push(aga, 7, 4);

    

    // struct Second
    // {
    //     Second(const std::string &s)
    //     {
    //         std::cout << "Second ctor\n";
    //         this->s = s;
    //     }
    //     Second(Second &&c)
    //     {
    //         std::cout << "Second move ctor\n";
    //         s = std::move(c.s);
    //     }
    //     Second(const Second &c)
    //     {
    //         std::cout << "Second copy ctor\n";
    //         this->s = c.s;
    //     };
    //     ~Second() { std::cout << "Second dtor\n"; }
    //     void operator()() const
    //     {
    //     }

    //   private:
    //     std::string s;
    // } second(", functor");
    // pool.push(std::ref(second));
    // // this_thread::sleep_for(chrono::milliseconds(2000));
    // // pool.push(const_cast<const Second &>(second));
    // pool.push(move(second));
    // pool.push(second);
    // pool.push(Second(", functor"));

    // auto f = pool.pop();

    // if (f)
    // {
    //     std::cout << "poped function from the pool ";
    //     f();
    // }


    // pool.resize(4);
    
    // std::string s2 = "result";
    // auto f1 = pool.push([s2](){
    //     return s2;
    // });
    // std::cout << "returned " << f1.get() << '\n';

    // auto f2 = pool.push([](){
    //     throw std::exception();
    // });
    // // other code here
    // //...
    // try {
    //     f2.get();
    // }
    // catch (std::exception & e) {
    //     std::cout << "caught exception\n";
    // }

    // // get thread 0
    // auto & th = pool.get_thread(0);


    return 0;
}
