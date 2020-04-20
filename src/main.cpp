#include <QImage>
#include <QCoreApplication>

#include <QColor>
#include <utility>
#include <cmath>
#include <memory>
#include <fstream>

#include <svgren/render.hpp>
#include <svgdom/elements/Gradients.hpp>
#include <papki/fs_file.hpp>

#include <nlohmann/json.hpp>

auto GetHsv(QColor rgb){
    int h, s, v, a;
    rgb.getHsv(&h, &s, &v, &a);
    return std::make_tuple(h,s,v,a);
}

auto ApplyGamma(int value, double gamma){
    return std::pow(value/255.0, gamma) * 255;
}


template<typename T>
class Image{
    private:
        std::unique_ptr<T[]> data;
        int w, h;
        
    public:
        Image(int w, int h, T init, std::unique_ptr<T[]> data) : w(w), h(h), data(std::move(data)) {}
        Image(int w, int h, T init) : w(w), h(h)
        {
            data = std::make_unique<T[]>( w * h );
            for(int i=0; i<w*h; i++)
                data[i] = init;
        }
        
        auto& operator()(int x, int y){
            return data[y*w + x];
        }
        auto& operator()(int x, int y) const { return data[y*w + x]; }
        auto width() const{ return w; }
        auto height() const{ return h; }
};

struct Color{
    int r,g,b,a;
    Color(int r=0, int g=0, int b=0, int a=255) : r(r), g(g), b(b), a(a) {}
    Color(QColor c) : r(c.red()), g(c.green()), b(c.blue()), a(c.alpha()) {}
    
    auto hue() const { return QColor::fromRgb(r,g,b,a).hue(); }
    auto sat() const { return QColor::fromRgb(r,g,b,a).saturation(); }
    auto val() const { return QColor::fromRgb(r,g,b,a).value(); }
    auto rgba() const { return qRgba(r, g, b, a); }
    
    static Color Hsv(int h, int s, int v, int a=255){
        return Color(QColor::fromHsv(std::clamp(h, 0, 359), std::clamp(s, 0, 255), std::clamp(v, 0, 255), std::clamp(a, 0, 255)));
    }
};

Image<Color> loadImage(QString path){
    QImage img(path);
    Image<Color> out(img.width(), img.height(), Color(0,0,0,0));
    
    for( int iy=0; iy<img.height(); iy++ )
        for( int ix=0; ix<img.width(); ix++ )
        {
            auto c = img.pixel( ix, iy );
            out(ix, iy) = {qRed(c), qGreen(c), qBlue(c), qAlpha(c)};
        }
        
    return out;
}

Image<Color> fromSvg(svgren::result result){
    int w = result.width;
    int h = result.height;
    Image<Color> out(w, h, Color(0,0,0,0));
    
    for( int iy=0; iy<h; iy++ )
        for( int ix=0; ix<w; ix++ )
        {
            auto rgba = result.pixels[iy*w + ix];
            auto a = int((rgba & 0xFF000000) >> 24);
            auto b = int((rgba & 0x00FF0000) >> 16);
            auto g = int((rgba & 0x0000FF00) >>  8);
            auto r = int((rgba & 0x000000FF) >>  0);
            out(ix, iy) = {r, g, b, a};
            //auto c = img.pixel( ix, iy );
            //out(ix, iy) = {qRed(c), qGreen(c), qBlue(c), qAlpha(c)};
        }
        
    return out;
}

void saveImage(QString path, const Image<Color>& img){
    QImage out( img.width(), img.height(), QImage::Format_ARGB32 );
    
    for( int iy=0; iy<img.height(); iy++ )
        for( int ix=0; ix<img.width(); ix++ )
            out.setPixel( ix, iy, img( ix, iy ).rgba() );
    
    out.save(path);
}

class WriteStyle : public svgdom::Visitor {
    public:
    std::string color;
    bool write;
    int count = 0;
    WriteStyle(std::string color, bool write) : color(color), write(write){}
    void visit(svgdom::Gradient::StopElement& e) override {
        if (write)
            e.styles[svgdom::StyleProperty_e::STOP_COLOR] = svgdom::StyleValue::parsePaint(color);
        count++;
    }
};

class VisitorTest : public svgdom::Visitor {
    public:
        std::map<std::string,std::string> replace_color;
        std::string match_id = "";
        bool write = false;
        bool replace = false;
        std::vector<std::string> layers;
        void visit(svgdom::LinearGradientElement& e) override {
            std::string color = (e.id == match_id) ? "#ffffff" : "#000000";
            auto key_it = replace_color.find(e.id);
            auto found = replace && (key_it != replace_color.end());
            if (found)
                color = key_it->second;
            
            WriteStyle visit(color, write || found);
            e.accept( visit );
            if (visit.count > 0)
                layers.push_back(e.id);
        }
};

template<typename T>
T mix(T a, T b, double amount){
    return static_cast<T>((1.0-amount)*a + amount*b);
}

using json = nlohmann::json;

auto getDefault = [](json& element, auto fallback){
    using T = decltype(fallback);
    return !element.empty() ? (element.get<T>()) : fallback;
};

class Adjustment{
    double fallback;
    const char* const name;
    Image<double> local;
    
    public:
        Adjustment(json& element, const char* const name, double fallback_value, int w, int h)
            :   fallback(getDefault(element[name], fallback_value))
            ,   name(name)
            ,   local(w, h, fallback)
            { }
            
        void update(json& element, Image<Color>& img_change){
            if(!element.empty()){
                auto val = getDefault(element[name], fallback);
                for( int iy=0; iy<img_change.height(); iy++ )
                    for( int ix=0; ix<img_change.width(); ix++ ){
                        auto amount = (img_change(ix, iy).r) / 255.0;
                        local(ix,iy) = mix(local(ix,iy), val, amount);
                    }
            }
        }
        
        double operator()(int x, int y){ return local(x, y); }
};

int main(int argc, char* argv[]){
    QCoreApplication app(argc, argv);
    
    auto args = app.arguments();
    if (args.size() == 1){
        std::cout << "Wrong arguments\n";
        return -1;
    }
    
    json js;
    std::ifstream(args[1].toUtf8().constData()) >> js;
    
    auto img_base = loadImage( js["base"].get<std::string>().c_str() );
    
    Adjustment gamma    (js, "gamma"   , 1.0, img_base.width(), img_base.height());
    Adjustment sat_boost(js, "satboost", 0.7, img_base.width(), img_base.height());
    Adjustment value_sub(js, "valsub"  , 0.0, img_base.width(), img_base.height());
    
    auto dom = svgdom::load(papki::fs_file(js["svg"]));
    
    VisitorTest test;
    test.write = false;
    dom->accept(test);
    
    for(auto layer : test.layers)
        std::cout << "Color: " << layer << std::endl;
    
    //TODO: Overwrite colors
    for(auto over : js["overrides"] ){
        auto id = over["id"].get<std::string>();
        if( !over["color"].empty() )
            test.replace_color.insert({id, over["color"].get<std::string>()});
    }
    
    test.replace = true;
    dom->accept(test);
    
    auto img_colors = fromSvg( svgren::render(*dom) );
    saveImage( "render.png", img_colors );
    
    for(auto over : js["overrides"] ){
        auto id = over["id"].get<std::string>();
        std::cout << "Overriding: " <<  id << std::endl;
        
        VisitorTest writer;
        writer.write = true;
        writer.match_id = id;
        dom->accept(writer);
        auto img_change = fromSvg( svgren::render(*dom) );
        //saveImage( ("test" + id + ".png").c_str(), img_change );
        
        gamma    .update( over, img_change );
        sat_boost.update( over, img_change );
        value_sub.update( over, img_change );
    }
    
    QImage out( img_base.width(), img_base.height(), QImage::Format_ARGB32 );
    
    for( int iy=0; iy<out.height(); iy++ )
        for( int ix=0; ix<out.width(); ix++ )
        {
            auto c = img_colors( ix, iy );
            auto b = img_base( ix, iy );
            
            auto h = c.hue();
            auto s = c.sat();
            auto v_sub = c.val();
            auto v = b.val();
            auto a = b.a;
            
            auto amount = (255 - v) / 255.0;
            s = s + s*amount*sat_boost(ix, iy);
            v = ApplyGamma(v, gamma(ix, iy));
            
            v -= value_sub(ix, iy)*(255 - v_sub);
            
            out.setPixel( ix, iy, Color::Hsv(h, s, v, a).rgba() );
        }
    
    out.save( "output.png" );
    return 0;
}