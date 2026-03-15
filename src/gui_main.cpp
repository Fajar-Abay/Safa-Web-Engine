#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include "Server.hpp"
#include "Logger.hpp"
#include <thread>
#include <memory>
#include <climits>
#include <cstdlib>
#include <chrono>
#include <sys/stat.h>

using namespace sf;

// ======================================================================
// WARNA PALETTE PREMIUM DARK THEME
// ======================================================================
namespace Colors {
    const Color BG          (22, 22, 30);
    const Color Panel       (30, 30, 42);
    const Color PanelBorder (50, 50, 70);
    const Color InputBG     (38, 38, 52);
    const Color InputBorder (65, 65, 90);
    const Color InputFocus  (100, 180, 255);
    const Color Cyan        (100, 210, 255);
    const Color Green       (80, 220, 100);
    const Color Red         (255, 90, 90);
    const Color Orange      (255, 180, 60);
    const Color Purple      (180, 130, 255);
    const Color TextWhite   (230, 230, 240);
    const Color TextDim     (130, 130, 160);
    const Color LogBG       (18, 18, 26);
    const Color BtnGreen    (40, 160, 70);
    const Color BtnRed      (180, 45, 45);
}

// ======================================================================
// UI Helper
// ======================================================================
void drawPanel(RenderWindow& win, float x, float y, float w, float h, Color bg, Color border) {
    RectangleShape r(Vector2f(w, h));
    r.setPosition(x, y);
    r.setFillColor(bg);
    r.setOutlineThickness(1);
    r.setOutlineColor(border);
    win.draw(r);
}

struct GUIButton {
    RectangleShape shape;
    Text label;
    Color normalColor, hoverColor;
    bool isHovered = false;

    GUIButton() = default;

    void create(float x, float y, float w, float h, const std::string& text,
                Font& font, Color normal, Color hover) {
        normalColor = normal; hoverColor = hover;
        shape.setPosition(x, y);
        shape.setSize(Vector2f(w, h));
        shape.setFillColor(normal);
        shape.setOutlineThickness(1);
        shape.setOutlineColor(Color(255, 255, 255, 40));

        label.setFont(font);
        label.setString(text);
        label.setCharacterSize(14);
        label.setStyle(Text::Bold);
        label.setFillColor(Color::White);
        centerLabel();
    }

    void centerLabel() {
        FloatRect b = label.getLocalBounds();
        label.setPosition(
            shape.getPosition().x + (shape.getSize().x - b.width) / 2,
            shape.getPosition().y + (shape.getSize().y - b.height) / 2 - 3);
    }

    void setText(const std::string& s, Color bg) {
        label.setString(s);
        normalColor = bg;
        shape.setFillColor(bg);
        centerLabel();
    }

    void updateHover(Vector2i mouse) {
        isHovered = shape.getGlobalBounds().contains(Vector2f(mouse));
        shape.setFillColor(isHovered ? hoverColor : normalColor);
    }

    void draw(RenderWindow& w) { w.draw(shape); w.draw(label); }

    bool clicked(Vector2i m) {
        return shape.getGlobalBounds().contains(Vector2f(m));
    }
};

struct GUIInput {
    RectangleShape box;
    Text content;
    Text label;
    std::string value;
    bool focused = false;

    GUIInput() = default;

    void create(float x, float y, float w, const std::string& lbl,
                const std::string& def, Font& font) {
        value = def;
        label.setFont(font); label.setString(lbl);
        label.setCharacterSize(11); label.setFillColor(Colors::TextDim);
        label.setPosition(x, y);

        box.setPosition(x, y + 16);
        box.setSize(Vector2f(w, 28));
        box.setFillColor(Colors::InputBG);
        box.setOutlineThickness(1.0f);
        box.setOutlineColor(Colors::InputBorder);

        content.setFont(font); content.setCharacterSize(13);
        content.setFillColor(Colors::TextWhite);
        content.setPosition(x + 6, y + 21);
    }

    void draw(RenderWindow& w) {
        box.setOutlineColor(focused ? Colors::InputFocus : Colors::InputBorder);
        w.draw(box); w.draw(label);
        content.setString(value + (focused ? "|" : ""));
        w.draw(content);
    }

    bool clicked(Vector2i m) { return box.getGlobalBounds().contains(Vector2f(m)); }

    void type(sf::Uint32 ch) {
        if (!focused) return;
        if (ch == 8 && !value.empty()) value.pop_back();
        else if (ch >= 32 && ch < 127 && value.size() < 100) value += static_cast<char>(ch);
    }
};

int main() {
    RenderWindow window(VideoMode(900, 680), "Safa Web Server - Dashboard", Style::Close);
    window.setFramerateLimit(24);

    Font font, fontBold;
    if (!font.loadFromFile("/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf")) font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    if (!fontBold.loadFromFile("/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf")) fontBold.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");

    std::unique_ptr<Server> srv;
    std::thread srvThread;
    bool running = false;
    auto startTime = std::chrono::steady_clock::now();

    // Branding
    Text brand("SAFA SERVER", fontBold, 28);
    brand.setFillColor(Colors::Cyan);
    brand.setPosition(30, 20);

    // Status Indicator
    CircleShape statusDot(6);
    statusDot.setPosition(30, 85);
    statusDot.setFillColor(Colors::Red);

    Text statusLabel("Offline", font, 16);
    statusLabel.setFillColor(Colors::Red);
    statusLabel.setPosition(50, 80);

    // Default path detection
    std::string defaultPath = "./www";
    struct stat st;
    if (stat("/var/www/safa-web-server", &st) == 0 && S_ISDIR(st.st_mode)) {
        defaultPath = "/var/www/safa-web-server";
    }

    // Config Panel
    float cfgY = 130;
    GUIInput inputPort, inputThreads, inputDocRoot, inputCert, inputKey;
    inputPort.create(30, cfgY, 100, "PORT", "8000", font);
    inputThreads.create(145, cfgY, 70, "THREADS", "4", font);
    inputDocRoot.create(230, cfgY, 350, "DOCUMENT ROOT", defaultPath, font);
    inputCert.create(595, cfgY, 275, "SSL CERT (HTTPS)", "", font);
    inputKey.create(30, cfgY + 65, 255, "SSL KEY (HTTPS)", "", font);

    std::vector<GUIInput*> allInputs = {&inputPort, &inputThreads, &inputDocRoot, &inputCert, &inputKey};

    GUIButton btnToggle;
    btnToggle.create(595, cfgY + 81, 275, 28, "START SERVER", fontBold, Colors::BtnGreen, Color(50, 190, 90));

    GUIButton btnOpenFolder;
    btnOpenFolder.create(230, cfgY + 81, 350, 28, "BUKA FOLDER WWW", fontBold, Colors::Panel, Colors::InputBorder);
    btnOpenFolder.label.setFillColor(Colors::Cyan);
    btnOpenFolder.centerLabel();

    while (window.isOpen()) {
        Event ev;
        while (window.pollEvent(ev)) {
            if (ev.type == Event::Closed) {
                if (running && srv) { srv->stop(); srvThread.join(); }
                window.close();
            }

            if (ev.type == Event::MouseButtonPressed && ev.mouseButton.button == Mouse::Left) {
                Vector2i mp = Mouse::getPosition(window);
                for (auto* inp : allInputs) inp->focused = !running && inp->clicked(mp);

                if (btnOpenFolder.clicked(mp)) {
                    std::string cmd = "xdg-open " + inputDocRoot.value + " &";
                    system(cmd.c_str());
                }

                if (btnToggle.clicked(mp)) {
                    if (!running) {
                        int port = 8000; int threads = 4;
                        try { port = std::stoi(inputPort.value); } catch (...) {}
                        try { threads = std::stoi(inputThreads.value); } catch (...) {}

                        char resolved[PATH_MAX];
                        std::string dr = inputDocRoot.value;
                        if (realpath(dr.c_str(), resolved) == nullptr) {
                            std::string mk = "mkdir -p " + dr;
                            system(mk.c_str());
                            if (realpath(dr.c_str(), resolved) == nullptr) {
                                Logger::error("Path invalid: " + dr);
                                goto next;
                            }
                        }
                        {
                            srv = std::make_unique<Server>(port, resolved, threads, inputCert.value, inputKey.value);
                            srvThread = std::thread([&]() { srv->start(); });
                            running = true;
                            startTime = std::chrono::steady_clock::now();
                            statusDot.setFillColor(Colors::Green);
                            statusLabel.setString("Online: localhost:" + std::to_string(port));
                            statusLabel.setFillColor(Colors::Green);
                            btnToggle.setText("STOP SERVER", Colors::BtnRed);
                        }
                        next:;
                    } else {
                        srv->stop();
                        if (srvThread.joinable()) srvThread.join();
                        srv.reset();
                        running = false;
                        statusDot.setFillColor(Colors::Red);
                        statusLabel.setString("Offline");
                        statusLabel.setFillColor(Colors::Red);
                        btnToggle.setText("START SERVER", Colors::BtnGreen);
                    }
                }
            }

            if (ev.type == Event::TextEntered && !running) {
                for (auto* inp : allInputs) inp->type(ev.text.unicode);
            }
        }

        Vector2i m = Mouse::getPosition(window);
        btnToggle.updateHover(m);
        btnOpenFolder.updateHover(m);

        window.clear(Colors::BG);
        drawPanel(window, 0, 0, 900, 75, Colors::Panel, Colors::PanelBorder);
        window.draw(brand); window.draw(statusDot); window.draw(statusLabel);

        drawPanel(window, 15, cfgY - 15, 870, 145, Colors::Panel, Colors::PanelBorder);
        for (auto* inp : allInputs) inp->draw(window);
        btnToggle.draw(window);
        btnOpenFolder.draw(window);

        // Logs
        drawPanel(window, 15, cfgY + 145, 870, 360, Colors::LogBG, Colors::PanelBorder);
        auto logs = Logger::get_recent_logs(20);
        float ly = cfgY + 155;
        for (auto& l : logs) {
            Text lt(l, font, 11);
            if (l.find("ERROR") != std::string::npos) lt.setFillColor(Colors::Red);
            else if (l.find("GET") != std::string::npos) lt.setFillColor(Colors::Green);
            else lt.setFillColor(Colors::TextDim);
            lt.setPosition(25, ly);
            window.draw(lt);
            ly += 16;
        }

        window.display();
    }
    return 0;
}
