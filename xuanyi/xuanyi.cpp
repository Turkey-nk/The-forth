#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>      // Windows API
#include <graphics.h>     // EasyX 图形库
#include <stdio.h>        // 标准输入输出
#include <tchar.h>        // 宽字符支持
#include <mmsystem.h>     // 多媒体音频支持
#pragma comment(lib, "winmm.lib")  // 链接音频库

// 游戏窗口大小
#define WINDOW_WIDTH  1280   
#define WINDOW_HEIGHT 720    

// ARGB 颜色宏定义 - 将四个通道合并为一个颜色值
#define ARGB(a, r, g, b) (( (a) << 24 ) | ( (r) << 16 ) | ( (g) << 8 ) | (b))

//定义画面状态
enum GameState {
    STATE_TITLE,           // 标题画面
    STATE_INTRO,           // 剧情介绍画面
    STATE_GAME,            // 主游戏画面（调查阶段）
    STATE_QUESTION,        // 询问嫌疑人画面（选择问题）
    STATE_DIALOG,          // 对话画面（显示回答）
    STATE_ACCUSE,          // 指认真凶画面
    STATE_ENDING_WIN,      // 胜利结局画面
    STATE_ENDING_LOSE,     // 失败结局画面
    STATE_BACKGROUND_STORY // 人物背景故事画面
};

// 当前查看背景故事的人物索引 (0=夫人, 1=杰克, 2=管家)
int currentStoryCharacter = 0;

//嫌疑人
enum Suspect {
    NONE,           
    SUSPECT_WIFE,    
    SUSPECT_JACK,    
    SUSPECT_BUTLER   
};

//对话选项
struct DialogOption {
    TCHAR text[100];      
    TCHAR response[500];  
    bool requiresClue;   
    TCHAR requiredClue[50]; 
    bool revealsClue;     
    TCHAR newClue[200];  
};

//函数声明
void SetChineseFont(int height);                                     
void LoadImages();                                                   
void DrawTitleScreen();                                              
void DrawIntroScreen();                                              
void DrawGameScreen();                                               
void ShowClue(const TCHAR* title, const TCHAR* desc);                
void DrawQuestionScreen();                                           
void DrawDialogScreen();                                             
void DrawAccuseScreen();                                            
void DrawEndingScreen(bool isWin);                                  
void HandleMouseClick(int x, int y);                                 
void StartTransition(GameState newState);                            
void UpdateTransition();                                             
void DrawTransition();                                               
void DrawProgressBar(int x, int y, int width, int height, int current, int max); 
void PlaySoundEffect(const TCHAR* soundName);                        
void PlayBackgroundMusic();                                          
void StopBackgroundMusic();                                         
void ShowDialogResponse(const TCHAR* response, bool revealNewClue, const TCHAR* newClueTitle, const TCHAR* newClueDesc); // 显示对话回答
void DrawBackgroundStoryScreen();                                    

//图片声明
IMAGE imgBackground;   
IMAGE imgWife;         
IMAGE imgJack;         
IMAGE imgButler;       
IMAGE imgCandle;      
IMAGE imgLetter;       
IMAGE imgScarf;        
IMAGE imgFootprint;    
IMAGE imgBody;         
IMAGE imgDialogBox;    
IMAGE imgTitle;        
IMAGE imgAccuseBtn;    
IMAGE imgWinEnd;       
IMAGE imgLoseEnd;      

GameState currentState = STATE_TITLE;  // 当前游戏状态
GameState nextState = STATE_TITLE;     // 下一个游戏状态（用于场景过渡）
Suspect currentSuspect = NONE;         // 当前正在询问的嫌疑人

//线索状态（是否收集）
bool clueCandle = false;   
bool clueLetter = false;    
bool clueScarf = false;    
bool clueBody = false;     
bool clueFootprint = false; 
int keyClueCount = 0;       

//人物是否询问
bool wifeInterrogated = false;   
bool jackInterrogated = false;   
bool butlerInterrogated = false; 

// 各人物的问题询问状态
bool wifeQuestion1 = false;  
bool wifeQuestion2 = false; 
bool wifeQuestion3 = false; 
bool jackQuestion1 = false;  
bool jackQuestion2 = false;  
bool jackQuestion3 = false;  
bool butlerQuestion1 = false; 
bool butlerQuestion2 = false; 
bool butlerQuestion3 = false; 

//场景过渡状态设置
bool isTransitioning = false;
int transitionAlpha = 255; 

//鼠标
int mouseX = 0, mouseY = 0;       
bool isHoveringButton = false;   

const int TOTAL_KEY_CLUES = 2;     // 指认真凶需要的关键线索数量

bool IsPointInRect(int x, int y, int rx, int ry, int rw, int rh); // 点是否在矩形内

//音频
void PlaySoundEffect(const TCHAR* soundName) {
    TCHAR soundPath[MAX_PATH];
    _stprintf_s(soundPath, MAX_PATH, _T("..\\..\\audio\\%s.wav"), soundName);
    PlaySound(soundPath, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

// 播放背景音乐（循环播放）
void PlayBackgroundMusic() {
    TCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    // 获取项目根目录路径（向上回溯三层）
    TCHAR* lastBackslash = _tcsrchr(exePath, _T('\\'));
    if (lastBackslash) *lastBackslash = _T('\0');
    TCHAR* secondBackslash = _tcsrchr(exePath, _T('\\'));
    if (secondBackslash) *secondBackslash = _T('\0');
    TCHAR* thirdBackslash = _tcsrchr(exePath, _T('\\'));
    if (thirdBackslash) *thirdBackslash = _T('\0');

    TCHAR musicPath[MAX_PATH];
    _stprintf_s(musicPath, MAX_PATH, _T("%s\\audio\\background.wav"), exePath);
    PlaySound(musicPath, NULL, SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
}

// 停止背景音乐
void StopBackgroundMusic() {
    PlaySound(NULL, NULL, 0);
}

//场景过渡
void StartTransition(GameState newState) {
    nextState = newState;
    isTransitioning = true;
    transitionAlpha = 0;
    PlaySoundEffect(_T("transition"));
}

// 更新过渡效果（淡入淡出）
void UpdateTransition() {
    if (!isTransitioning) return;

    transitionAlpha += 15; 
    if (transitionAlpha >= 255) {
        transitionAlpha = 255;
        currentState = nextState; 
        isTransitioning = false;
    }
}

// 绘制过渡效果（全屏淡入）
void DrawTransition() {
    if (!isTransitioning) return;
    setfillcolor(ARGB(transitionAlpha, 0, 0, 0));
    solidrectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
}

// 绘制进度条
void DrawProgressBar(int x, int y, int width, int height, int current, int max) {
    setfillcolor(RGB(50, 50, 50));
    solidrectangle(x, y, x + width, y + height);
    int progressWidth = (int)((float)current / max * width);
    if (progressWidth > 0) {
        setfillcolor(RGB(255, 200, 50));
        solidrectangle(x + 2, y + 2, x + progressWidth - 2, y + height - 2);
    }
    
    // 绘制进度文字
    TCHAR text[50];
    _stprintf_s(text, 50, _T("%d/%d"), current, max);
    SetChineseFont(height - 4);
    settextcolor(RGB(255, 255, 255));
    outtextxy(x + (width - textwidth(text)) / 2, y + 2, text);
}

// 加载所有图片资源
void LoadImages() {
    TCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    // 获取项目根目录路径（向上回溯三层）
    TCHAR* lastBackslash = _tcsrchr(exePath, _T('\\'));
    if (lastBackslash) *lastBackslash = _T('\0');
    TCHAR* secondBackslash = _tcsrchr(exePath, _T('\\'));
    if (secondBackslash) *secondBackslash = _T('\0');
    TCHAR* thirdBackslash = _tcsrchr(exePath, _T('\\'));
    if (thirdBackslash) *thirdBackslash = _T('\0');

    TCHAR imgPath[MAX_PATH];
    _stprintf_s(imgPath, MAX_PATH, _T("%s\\image.png"), exePath);

    TCHAR fullPath[MAX_PATH];
    
    // 加载所需图片
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\background.png"), imgPath);
    loadimage(&imgBackground, fullPath, WINDOW_WIDTH, WINDOW_HEIGHT);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\伯爵夫人.png"), imgPath);
    loadimage(&imgWife, fullPath, 200, 200);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\养子.png"), imgPath);
    loadimage(&imgJack, fullPath, 200, 200);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\管家.png"), imgPath);
    loadimage(&imgButler, fullPath, 200, 200);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\烛台.png"), imgPath);
    loadimage(&imgCandle, fullPath, 90, 90);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\信件和遗嘱草稿.png"), imgPath);
    loadimage(&imgLetter, fullPath, 80, 55);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\女士羊毛围巾.png"), imgPath);
    loadimage(&imgScarf, fullPath, 75, 150);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\泥脚印.png"), imgPath);
    loadimage(&imgFootprint, fullPath, 120, 90);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\男爵尸体.png"), imgPath);
    loadimage(&imgBody, fullPath, 100, 150);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\对话框.png"), imgPath);
    loadimage(&imgDialogBox, fullPath, 600, 600);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\标题.png"), imgPath);
    loadimage(&imgTitle, fullPath, 400, 350);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\指认按钮—手铐.png"), imgPath);
    loadimage(&imgAccuseBtn, fullPath, 180, 180);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\胜利结算.png"), imgPath);
    loadimage(&imgWinEnd, fullPath, WINDOW_WIDTH, WINDOW_HEIGHT);
    _stprintf_s(fullPath, MAX_PATH, _T("%s\\失败结算.png"), imgPath);
    loadimage(&imgLoseEnd, fullPath, WINDOW_WIDTH, WINDOW_HEIGHT);
}

// 设置中文字体（微软雅黑）
void SetChineseFont(int height) {
    settextstyle(height, 0, _T("微软雅黑"));
}

// 绘制开场画面
void DrawTitleScreen() {
    cleardevice();  
    putimage(0, 0, &imgBackground); 
    int titleX = (WINDOW_WIDTH - 400) / 2;
    putimage(titleX, 100, &imgTitle);
    setbkmode(TRANSPARENT); 
    SetChineseFont(30);
    settextcolor(RGB(200, 180, 100));
    
    // 悬停效果
    if (isHoveringButton) {
        settextcolor(RGB(255, 255, 200));
        settextstyle(32, 0, _T("微软雅黑"));
    }
    outtextxy(950, 600, _T("点击任意处开始调查"));
}

// 剧情介绍画面
void DrawIntroScreen() {
    cleardevice();
    putimage(0, 0, &imgBackground);
    int boxWidth = 600;
    int boxHeight = 600;
    int boxX = (WINDOW_WIDTH - boxWidth) / 2;
    int boxY = (WINDOW_HEIGHT - boxHeight) / 2;
    putimage(boxX, boxY, &imgDialogBox);

    setbkmode(TRANSPARENT);
    SetChineseFont(24);
    settextcolor(RGB(220, 200, 150));

    // 显示剧情信息（居中对齐）
    TCHAR line1[] = _T("地点：郊外灰石古堡");
    int line1Width = textwidth(line1);
    outtextxy(boxX + (boxWidth - line1Width) / 2, boxY + 150, line1);

    TCHAR line2[] = _T("时间：月圆之夜，深夜11点");
    int line2Width = textwidth(line2);
    outtextxy(boxX + (boxWidth - line2Width) / 2, boxY + 220, line2);

    TCHAR line3[] = _T("受害者：老男爵（70岁），古堡主人");
    int line3Width = textwidth(line3);
    outtextxy(boxX + (boxWidth - line3Width) / 2, boxY + 290, line3);

    TCHAR line4[] = _T("你是：一位路过避雨的侦探");
    int line4Width = textwidth(line4);
    outtextxy(boxX + (boxWidth - line4Width) / 2, boxY + 360, line4);

    TCHAR line5[] = _T("点击继续...");
    int line5Width = textwidth(line5);
    outtextxy(boxX + (boxWidth - line5Width) / 2, boxY + 420, line5);
}

//绘制探案界面
void DrawGameScreen() {
    cleardevice();
    putimage(0, 0, &imgBackground);

    setbkmode(TRANSPARENT);
    SetChineseFont(35);
    settextcolor(RGB(255, 100, 100));
    outtextxy(450, 20, _T("古堡谜案 - 调查"));

    setfillcolor(RGB(30, 30, 40));
    solidrectangle(50, 620, 350, 690);
    
    SetChineseFont(20);
    settextcolor(RGB(200, 180, 100));
    outtextxy(60, 630, _T("关键线索收集进度"));
    
    DrawProgressBar(60, 655, 280, 25, keyClueCount, TOTAL_KEY_CLUES);

    //判断鼠标是否悬停
    bool hoverCandle = IsPointInRect(mouseX, mouseY, 480, 430, 90, 90);
    bool hoverLetter = IsPointInRect(mouseX, mouseY, 600, 440, 80, 55);
    bool hoverScarf = IsPointInRect(mouseX, mouseY, 810, 420, 75, 150);
    bool hoverFootprint = IsPointInRect(mouseX, mouseY, 280, 620, 120, 90);
    bool hoverWife = IsPointInRect(mouseX, mouseY, 50, 30, 200, 200);
    bool hoverBody= IsPointInRect(mouseX, mouseY, 595, 545, 100, 150);
    bool hoverJack = IsPointInRect(mouseX, mouseY, 50, 260, 200, 200);
    bool hoverButler = IsPointInRect(mouseX, mouseY, 950, 150, 200, 200);
    bool hoverAccuse = IsPointInRect(mouseX, mouseY, WINDOW_WIDTH - 220, 520, 180, 180);

    if (hoverCandle || hoverLetter || hoverScarf || hoverFootprint || 
        hoverWife || hoverJack || hoverButler || hoverAccuse|| hoverBody) {
        isHoveringButton = true;
    } else {
        isHoveringButton = false;
    }

    //悬停高亮
    if (hoverCandle) {
        setfillcolor(ARGB(100, 255, 200, 100));
        solidrectangle(475, 425, 575, 530);
    }
    putimage(480, 430, &imgCandle);
    SetChineseFont(18);
    settextcolor(hoverCandle ? RGB(255, 255, 200) : RGB(200, 180, 100));
    outtextxy(495, 530, _T("烛台"));

    //查看线索后标记
    if (clueCandle) {
        settextcolor(RGB(0, 255, 0));
        outtextxy(520, 555, _T("✓"));
    }

    if (hoverLetter) {
        setfillcolor(ARGB(100, 255, 200, 100));
        solidrectangle(595, 435, 685, 505);
    }
    putimage(600, 440, &imgLetter);
    settextcolor(hoverLetter ? RGB(255, 255, 200) : RGB(200, 180, 100));
    outtextxy(615, 510, _T("信件遗嘱"));
    if (clueLetter) {
        settextcolor(RGB(0, 255, 0));
        outtextxy(645, 535, _T("✓"));
    }

    if (hoverScarf) {
        setfillcolor(ARGB(100, 255, 200, 100));
        solidrectangle(805, 415, 890, 580);
    }
    putimage(810, 420, &imgScarf);
    settextcolor(hoverScarf ? RGB(255, 255, 200) : RGB(200, 180, 100));
    outtextxy(820, 580, _T("女士围巾"));
    if (clueScarf) {
        settextcolor(RGB(0, 255, 0));
        outtextxy(840, 605, _T("✓"));
    }

    if (hoverFootprint) {
        setfillcolor(ARGB(100, 255, 200, 100));
        solidrectangle(275, 615, 405, 715);
    }
    putimage(280, 620, &imgFootprint);
    settextcolor(hoverFootprint ? RGB(255, 255, 200) : RGB(200, 180, 100));
    outtextxy(290, 715, _T("泥脚印"));
    if (clueFootprint) {
        settextcolor(RGB(0, 255, 0));
        outtextxy(335, 740, _T("✓"));
    }

    if (hoverWife) {
        setfillcolor(ARGB(100, 255, 200, 100));
        solidrectangle(45, 25, 250, 245);
    }
    putimage(50, 30, &imgWife);
    settextcolor(hoverWife ? RGB(255, 255, 200) : RGB(200, 180, 100));
    outtextxy(55, 240, _T("男爵夫人"));
    if (wifeInterrogated) {
        settextcolor(RGB(0, 255, 0));
        outtextxy(120, 265, _T("[已询问]"));
    }

    if (hoverJack) {
        setfillcolor(ARGB(100, 255, 200, 100));
        solidrectangle(45, 255, 250, 475);
    }
    putimage(50, 260, &imgJack);
    settextcolor(hoverJack ? RGB(255, 255, 200) : RGB(200, 180, 100));
    outtextxy(55, 470, _T("养子杰克"));
    if (jackInterrogated) {
        settextcolor(RGB(0, 255, 0));
        outtextxy(120, 495, _T("[已询问]"));
    }

    if (hoverButler) {
        setfillcolor(ARGB(100, 255, 200, 100));
        solidrectangle(945, 145, 1150, 365);
    }
    putimage(950, 150, &imgButler);
    settextcolor(hoverButler ? RGB(255, 255, 200) : RGB(200, 180, 100));
    outtextxy(955, 360, _T("老管家"));
    if (butlerInterrogated) {
        settextcolor(RGB(0, 255, 0));
        outtextxy(1020, 385, _T("[已询问]"));
    }

    if (hoverBody) {
        setfillcolor(ARGB(100, 255, 200, 100));
        solidrectangle(590, 540, 595 + 100 + 5, 545 + 150 + 5); 
    }
    putimage(595, 545, &imgBody);
    SetChineseFont(18); 
    settextcolor(hoverBody ? RGB(255, 255, 200) : RGB(200, 180, 100));
    outtextxy(620, 700, _T("男爵尸体"));
    if (clueBody) {
        settextcolor(RGB(0, 255, 0));
        outtextxy(650, 715, _T("✓")); 
    }

    if (hoverAccuse) {
        setfillcolor(ARGB(100, 255, 100, 100));
        solidrectangle(WINDOW_WIDTH - 225, 515, WINDOW_WIDTH - 35, 705);
    }

    putimage(WINDOW_WIDTH - 220, 520, &imgAccuseBtn);
    SetChineseFont(18);
    settextcolor(hoverAccuse ? RGB(255, 100, 100) : RGB(200, 180, 100));
    outtextxy(WINDOW_WIDTH - 190, 710, _T("指认真凶"));
}

//展示线索
void ShowClue(const TCHAR* title, const TCHAR* desc) {
    cleardevice();//清屏
    putimage(0, 0, &imgBackground);

    int boxWidth = 600;
    int boxHeight = 600;
    int boxX = (WINDOW_WIDTH - boxWidth) / 2;
    int boxY = (WINDOW_HEIGHT - boxHeight) / 2;

    // 绘制一个由多个同心圆组成的渐变发光效果
    for (int i = 0; i < 3; i++) {
        int alpha = 80 + i * 40;
        setfillcolor(ARGB(alpha, 255, 255, 200));
        solidcircle(boxX + boxWidth / 2, boxY + boxHeight / 2, 350 - i * 50);
        Sleep(50);
        FlushBatchDraw();
    }

    putimage(boxX, boxY, &imgDialogBox);

    int innerY = boxY + 120;

    setbkmode(TRANSPARENT);
    SetChineseFont(32);
    settextcolor(RGB(255, 220, 100));//标题颜色

    int titleWidth = textwidth(title);
    int titleX = boxX + (boxWidth - titleWidth) / 2;
    outtextxy(titleX, innerY+50 , title);//标题位置

    SetChineseFont(24);
    settextcolor(RGB(220, 200, 160));

    int descWidth = textwidth(desc);
    int descX = boxX + (boxWidth - descWidth) / 2;
    outtextxy(descX, innerY + 150, desc);

    SetChineseFont(22);
    settextcolor(RGB(255, 255, 200));

    TCHAR continueText[] = _T("【点击继续】");
    int continueWidth = textwidth(continueText);
    int continueX = boxX + (boxWidth - continueWidth) / 2;
    outtextxy(continueX, innerY + 250, continueText);

    FlushBatchDraw();

    //等待鼠标动态进行下一步
    bool waiting = true;
    while (waiting) {
        Sleep(30);
        if (MouseHit()) {
            MOUSEMSG msg = GetMouseMsg();
            if (msg.uMsg == WM_LBUTTONDOWN) {
                waiting = false;
            }
        }
    }
}

//询问嫌疑人
void DrawQuestionScreen() {
    cleardevice();
    putimage(0, 0, &imgBackground);

    setbkmode(TRANSPARENT);//文字背景透明
    SetChineseFont(35);
    settextcolor(RGB(255, 100, 100));
    outtextxy(450, 20, _T("古堡谜案 - 询问"));

    int x = 100;
    if (currentSuspect == SUSPECT_WIFE) {
        putimage(x, 100, &imgWife);
        SetChineseFont(28);
        settextcolor(RGB(255, 220, 100));
        outtextxy(480, 150, _T("男爵夫人（45岁）"));

        SetChineseFont(22);
        settextcolor(RGB(200, 200, 180));
        outtextxy(480, 210, _T("关系：老男爵的第二任妻子"));
        outtextxy(480, 260, _T("动机：老男爵准备修改遗嘱"));
        outtextxy(480, 310, _T("证词：我在卧室看书，什么都不知道"));
    } else if (currentSuspect == SUSPECT_JACK) {
        putimage(x, 100, &imgJack);
        SetChineseFont(28);
        settextcolor(RGB(255, 220, 100));
        outtextxy(480, 150, _T("养子杰克（28岁）"));

        SetChineseFont(22);
        settextcolor(RGB(200, 200, 180));
        outtextxy(480, 210, _T("关系：老男爵收养的儿子"));
        outtextxy(480, 260, _T("动机：经营不善亏了很多钱"));
        outtextxy(480, 310, _T("证词：我承认和男爵吵架了，但人不是我杀的"));
    } else if (currentSuspect == SUSPECT_BUTLER) {
        putimage(x, 100, &imgButler);
        SetChineseFont(28);
        settextcolor(RGB(255, 220, 100));
        outtextxy(480, 150, _T("老管家（60岁）"));

        SetChineseFont(22);
        settextcolor(RGB(200, 200, 180));
        outtextxy(480, 210, _T("关系：在古堡工作30年"));
        outtextxy(480, 260, _T("动机：女儿生病需要钱"));
        outtextxy(480, 310, _T("证词：我看到杰克从书房出来"));
    }

    SetChineseFont(24);
    settextcolor(RGB(200, 180, 100));
    outtextxy(480, 380, _T("选择要询问的问题："));

    //询问选项的信息
    int optionY = 430;
    int optionWidth = 500;
    int optionHeight = 45;

    //不同的响应情节
    DialogOption wifeOptions[] = {
        {_T("1. 案发当晚你在哪里？"), _T("「我一直在卧室看书，外面下着大雨，我哪儿也没去。」（眼神有些闪烁）"), false, _T(""), false, _T("")},
        {_T("2. 关于遗嘱你知道些什么？"), _T("「我...我听说老爷要修改遗嘱，这对我很不利...但我绝不会做出那种事！」（情绪激动）"), true, _T("letter"), true, _T("夫人的围巾上检测到与书房相同的灰尘！")},
        {_T("3. 你的围巾为什么会在书房？"), _T("「这...这一定是有人栽赃！我从没去过书房！」（声音发抖）"), true, _T("scarf"), false, _T("")}
    };

    DialogOption jackOptions[] = {
        {_T("1. 你和男爵吵架是怎么回事？"), _T("「那个老顽固！他要把财产都捐出去，一分都不给我！我是他唯一的儿子啊！」（愤怒）"), false, _T(""), false, _T("")},
        {_T("2. 案发时你在做什么？"), _T("「我在自己房间处理账目，喝了点酒，后来就睡着了。」（眼神躲闪）"), false, _T(""), false, _T("")},
        {_T("3. 管家说看到你从书房出来？"), _T("「他在撒谎！我根本没去过书房！他肯定是想嫁祸给我！」（激动否认）"), true, _T("footprint"), false, _T("")}
    };

    DialogOption butlerOptions[] = {
        {_T("1. 案发当晚你看到了什么？"), _T("「我在厨房准备夜宵，看到杰克先生从书房方向匆匆走出来。他的表情...很奇怪。」"), false, _T(""), false, _T("")},
        {_T("2. 你女儿的病情怎么样了？"), _T("「...她需要手术，但费用很高。我向老爷求助，可他...他拒绝了。」（眼眶发红）"), false, _T(""), false, _T("")},
        {_T("3. 烛台上为什么有你的指纹？"), _T("「这...这不可能！我...我只是...」（脸色苍白，支支吾吾）"), true, _T("candle"), true, _T("")}
    };

    // 指向当前嫌疑人对应的选项数组和问题已询问状态数组
    DialogOption* options = nullptr;
    int optionCount = 0;
    bool* questions = nullptr;

    if (currentSuspect == SUSPECT_WIFE) {
        options = wifeOptions;
        optionCount = 3;
        bool temp[] = {wifeQuestion1, wifeQuestion2, wifeQuestion3};
        questions = new bool[3];
        for (int i = 0; i < 3; i++) questions[i] = temp[i];
    } else if (currentSuspect == SUSPECT_JACK) {
        options = jackOptions;
        optionCount = 3;
        bool temp[] = {jackQuestion1, jackQuestion2, jackQuestion3};
        questions = new bool[3];
        for (int i = 0; i < 3; i++) questions[i] = temp[i];
    } else if (currentSuspect == SUSPECT_BUTLER) {
        options = butlerOptions;
        optionCount = 3;
        bool temp[] = {butlerQuestion1, butlerQuestion2, butlerQuestion3};
        questions = new bool[3];
        for (int i = 0; i < 3; i++) questions[i] = temp[i];
    }

    //循环每个询问选项
    for (int i = 0; i < optionCount; i++) {
        int optionX = (WINDOW_WIDTH - optionWidth) / 2;
        bool requiresClue = options[i].requiresClue;
        bool hasClue = false;
        
        //判断线索是否足够询问问题
        if (requiresClue) {
            if (_tcscmp(options[i].requiredClue, _T("letter")) == 0 && clueLetter) hasClue = true;
            if (_tcscmp(options[i].requiredClue, _T("scarf")) == 0 && clueScarf) hasClue = true;
            if (_tcscmp(options[i].requiredClue, _T("candle")) == 0 && clueCandle) hasClue = true;
            if (_tcscmp(options[i].requiredClue, _T("footprint")) == 0 && clueFootprint) hasClue = true;
        } else {
            hasClue = true;
        }

        bool hover = IsPointInRect(mouseX, mouseY, optionX, optionY, optionWidth, optionHeight);
        
        //根据不同状态设置背景色即为悬停高亮
        if (hover && hasClue) {
            setfillcolor(ARGB(100, 255, 200, 100));
        } else if (questions[i]) {
            setfillcolor(ARGB(80, 100, 150, 100));
        } else if (!hasClue) {
            setfillcolor(ARGB(80, 80, 80, 80));
        } else {
            setfillcolor(ARGB(80, 100, 100, 120));
        }
        solidrectangle(optionX, optionY, optionX + optionWidth, optionY + optionHeight);
        
        //根据不同状态设置文本颜色
        if (questions[i]) {
            settextcolor(RGB(150, 200, 150));
        } else if (!hasClue) {
            settextcolor(RGB(100, 100, 100));
        } else {
            settextcolor(hover ? RGB(255, 255, 255) : RGB(200, 200, 180));
        }
        
        SetChineseFont(22);//字体
        TCHAR optionText[120];
        
        //问题状态
        if (questions[i]) {
            _stprintf_s(optionText, 120, _T("%s [已询问]"), options[i].text);
        } else if (!hasClue) {
            _stprintf_s(optionText, 120, _T("%s [需要线索]"), options[i].text);
        } else {
            _tcscpy_s(optionText, 120, options[i].text);
        }
        outtextxy(optionX + 15, optionY + 10, optionText);

        optionY += 60;
    }

    delete[] questions;//释放动态分配的内存

    //绘制返回按钮
    bool hoverBack = IsPointInRect(mouseX, mouseY, WINDOW_WIDTH - 200, 650, 180, 40);
    SetChineseFont(24);
    settextcolor(hoverBack ? RGB(255, 255, 255) : RGB(255, 255, 200));
    outtextxy(WINDOW_WIDTH - 200, 650, _T("返回调查"));
    
    if (hoverBack) {
        setfillcolor(ARGB(80, 255, 255, 200));
        solidrectangle(WINDOW_WIDTH - 205, 645, WINDOW_WIDTH - 15, 695);
    }
}

//绘制对话时的界面
void ShowDialogResponse(const TCHAR* response, bool revealNewClue, const TCHAR* newClueTitle, const TCHAR* newClueDesc) {
    cleardevice();
    putimage(0, 0, &imgBackground);

    int boxWidth = 600;
    int boxHeight = 600;
    int boxX = (WINDOW_WIDTH - boxWidth) / 2;
    int boxY = (WINDOW_HEIGHT - boxHeight) / 2;

    putimage(boxX, boxY, &imgDialogBox);

    setbkmode(TRANSPARENT);
    SetChineseFont(26);
    settextcolor(RGB(220, 200, 160));

    int innerY = boxY + 120+90;
    int lineHeight = 38;
    int charsPerLine = 30;
    
    const TCHAR* p = response;
    TCHAR line[50];
    
    // 逐行绘制响应文本，进行简单换行处理
    while (*p && innerY < boxY + boxHeight - 120) {
        int i = 0;
        while (*p && i < charsPerLine) {
            if (*p == _T('。') || *p == _T('！') || *p == _T('？') || *p == _T('」')) {
                line[i++] = *p++;
                break;
            }
            line[i++] = *p++;
        }
        line[i] = _T('\0');
        
        outtextxy(boxX + 200, innerY, line);
        innerY += lineHeight;
    }

    SetChineseFont(22);
    settextcolor(RGB(255, 255, 200));
    outtextxy(boxX + (boxWidth - 150) / 2, boxY + boxHeight - 80, _T("【点击继续】"));

    FlushBatchDraw();

    bool waiting = true;
    while (waiting) {
        Sleep(30);
        if (MouseHit()) {
            MOUSEMSG msg = GetMouseMsg();
            if (msg.uMsg == WM_LBUTTONDOWN) {
                waiting = false;
            }
        }
    }
}

//指认界面
void DrawAccuseScreen() {
    cleardevice();
    putimage(0, 0, &imgBackground);

    setbkmode(TRANSPARENT);
    SetChineseFont(40);
    settextcolor(RGB(255, 80, 80));
    outtextxy(500, 50, _T("指认真凶"));

    setfillcolor(RGB(30, 30, 40));
    solidrectangle(450, 120, 830, 170);
    
    SetChineseFont(25);
    settextcolor(RGB(200, 180, 100));
    outtextxy(460, 130, _T("已收集关键线索"));
    
    DrawProgressBar(560, 135, 250, 25, keyClueCount, TOTAL_KEY_CLUES);
    
    if (keyClueCount < TOTAL_KEY_CLUES) {
        SetChineseFont(20);
        settextcolor(RGB(255, 100, 100));
        outtextxy(520, 165, _T("提示：建议收集完所有关键线索再进行指认"));
    }

    //判断鼠标悬停
    bool hoverWife = IsPointInRect(mouseX, mouseY, 200, 280, 200, 200);
    bool hoverJack = IsPointInRect(mouseX, mouseY, 540, 280, 200, 200);
    bool hoverButler = IsPointInRect(mouseX, mouseY, 880, 280, 200, 200);
    bool hoverBack = IsPointInRect(mouseX, mouseY, WINDOW_WIDTH - 200, 650, 180, 40);

    //绘制嫌疑人并根据鼠标状态高亮
    if (hoverWife) {
        setfillcolor(ARGB(100, 255, 100, 100));
        solidrectangle(195, 275, 405, 495);
    }
    putimage(200, 280, &imgWife);
    SetChineseFont(22);
    settextcolor(hoverWife ? RGB(255, 200, 200) : RGB(255, 255, 255));
    outtextxy(230, 490, _T("男爵夫人"));

    if (hoverJack) {
        setfillcolor(ARGB(100, 255, 100, 100));
        solidrectangle(535, 275, 745, 495);
    }
    putimage(540, 280, &imgJack);
    settextcolor(hoverJack ? RGB(255, 200, 200) : RGB(255, 255, 255));
    outtextxy(580, 490, _T("养子杰克"));

    if (hoverButler) {
        setfillcolor(ARGB(100, 255, 100, 100));
        solidrectangle(875, 275, 1085, 495);
    }
    putimage(880, 280, &imgButler);
    settextcolor(hoverButler ? RGB(255, 200, 200) : RGB(255, 255, 255));
    outtextxy(920, 490, _T("老管家"));

    settextcolor(hoverBack ? RGB(255, 255, 255) : RGB(255, 255, 200));
    outtextxy(WINDOW_WIDTH - 200, 650, _T("返回调查"));
    
    if (hoverBack) {
        setfillcolor(ARGB(80, 255, 255, 200));
        solidrectangle(WINDOW_WIDTH - 205, 645, WINDOW_WIDTH - 15, 695);
    }
}

//结局界面
void DrawEndingScreen(bool isWin) {
    cleardevice();
    setbkmode(TRANSPARENT);

    if (isWin) {
        putimage(0, 0, &imgWinEnd);
        SetChineseFont(50);
        settextcolor(RGB(255, 220, 100));
        outtextxy(400, 100, _T("案件告破"));

        SetChineseFont(25);
        settextcolor(RGB(220, 200, 150));
        outtextxy(600, 200, _T("管家最终在证据面前认罪了！"));
        outtextxy(600, 250, _T("他的女儿身患重病，而男爵拒绝提供帮助..."));
        outtextxy(600, 300, _T("他抓住机会杀死了老男爵"));
        outtextxy(600, 350, _T("管家被逮捕，案件终于告破！"));

        SetChineseFont(22);
        settextcolor(RGB(200, 180, 100));
        outtextxy(50, 450, _T("点击人物查看背景故事："));

        bool hoverWife = IsPointInRect(mouseX, mouseY, 100, 500, 200, 200);
        bool hoverJack = IsPointInRect(mouseX, mouseY, 350, 500, 200, 200);
        bool hoverButler = IsPointInRect(mouseX, mouseY, 600, 500, 200, 200);

        if (hoverWife) {
            setfillcolor(ARGB(100, 100, 150, 255));
            solidrectangle(95, 495, 305, 705);
        }
        putimage(100, 500, &imgWife);
        settextcolor(hoverWife ? RGB(255, 200, 200) : RGB(255, 255, 255));
        outtextxy(120, 660, _T("男爵夫人"));

        if (hoverJack) {
            setfillcolor(ARGB(100, 100, 150, 255));
            solidrectangle(345, 495, 555, 705);
        }
        putimage(350, 500, &imgJack);
        settextcolor(hoverJack ? RGB(255, 200, 200) : RGB(255, 255, 255));
        outtextxy(310, 660, _T("养子杰克"));

        if (hoverButler) {
            setfillcolor(ARGB(100, 100, 150, 255));
            solidrectangle(595, 495, 805, 710);
        }
        putimage(600, 500, &imgButler);
        settextcolor(hoverButler ? RGB(255, 200, 200) : RGB(255, 255, 255));
        outtextxy(490, 660, _T("老管家"));
    } else {
        putimage(0, 0, &imgLoseEnd);
        SetChineseFont(50);
        settextcolor(RGB(200, 80, 80));
        outtextxy(450, 150, _T("指控错误"));

        SetChineseFont(28);
        settextcolor(RGB(200, 180, 150));
        outtextxy(600, 280, _T("真凶仍然逍遥法外..."));
        outtextxy(600, 330, _T("抓错人了！请再试一次..."));
    }

    SetChineseFont(24);
    settextcolor(RGB(255, 255, 200));
    outtextxy(450, 700, _T("[点击重新开始]"));
}

bool IsPointInRect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

//鼠标数据处理
void HandleMouseClick(int x, int y) {
    if (isTransitioning) return;
    
    PlaySoundEffect(_T("click"));
    
    switch (currentState) {
        case STATE_TITLE:
            StartTransition(STATE_INTRO);
            break;
        case STATE_INTRO:
            StartTransition(STATE_GAME);
            break;
        case STATE_GAME:
            if (IsPointInRect(x, y, 480, 430, 90, 90)) {
                if (!clueCandle) {
                    clueCandle = true;
                    keyClueCount++;//关键线索
                    PlaySoundEffect(_T("clue"));
                }
                ShowClue(_T("烛台（凶器）"), clueCandle ?
                    _T("烛台上检测到管家的指纹！") : _T("青铜烛台是凶器！"));
            } else if (IsPointInRect(x, y, 600, 440, 80, 55)) {
                if (!clueLetter) {
                    clueLetter = true;
                    PlaySoundEffect(_T("clue"));
                }
                ShowClue(_T("信件遗嘱"), _T("男爵准备修改遗嘱，剥夺养子继承权！"));
            } else if (IsPointInRect(x, y, 810, 420, 75, 150)) {
                if (!clueScarf) {
                    clueScarf = true;
                    PlaySoundEffect(_T("clue"));
                }
                ShowClue(_T("女士围巾"), _T("这是男爵夫人的物品，却出现在书房！"));
            } else if (IsPointInRect(x, y, 280, 620, 120, 90)) {
                if (!clueFootprint) {
                    clueFootprint = true;
                    PlaySoundEffect(_T("clue"));
                }
                ShowClue(_T("泥脚印"), _T("从大门到书房有一串泥脚印！"));
            }
            else if (IsPointInRect(x, y, 595, 545, 100, 150)) {
                if (!clueBody) {
                    clueBody = true;
                    keyClueCount++; 
                    PlaySoundEffect(_T("clue"));
                }
                ShowClue(_T("男爵尸体"), _T("男爵头部有钝器伤，死亡时间是昨晚11点左右。")); 
            }
            else if (IsPointInRect(x, y, 50, 30, 200, 200)) {
                wifeInterrogated = true;
                currentSuspect = SUSPECT_WIFE;
                StartTransition(STATE_QUESTION);
            } else if (IsPointInRect(x, y, 50, 260, 200, 200)) {
                jackInterrogated = true;
                currentSuspect = SUSPECT_JACK;
                StartTransition(STATE_QUESTION);
            } else if (IsPointInRect(x, y, 950, 150, 200, 200)) {
                butlerInterrogated = true;
                currentSuspect = SUSPECT_BUTLER;
                StartTransition(STATE_QUESTION);
            } else if (IsPointInRect(x, y, WINDOW_WIDTH - 220, 520, 180, 180)) {
                StartTransition(STATE_ACCUSE);
            }
            break;
        case STATE_QUESTION: {
            int optionWidth = 500;
            int optionHeight = 45;
            int optionY = 430;
            int optionX = (WINDOW_WIDTH - optionWidth) / 2;

            DialogOption wifeOptions[] = {
                {_T(""), _T("我一直在卧室看书，外面下着大雨，我哪儿也没去。(眼神有些闪烁，似乎在隐瞒什么)"), false, _T(""), false, _T("")},
                {_T(""), _T("我...我听说老爷要修改遗嘱...三年前我偷偷资助弟弟做生意，他卷款跑了，我欠了一大笔债...(眼眶泛红)"), true, _T("letter"), true, _T("发现夫人写给弟弟的信，信中提到已经没有退路了！")},
                {_T(""), _T("围巾...我确实去过书房找他求情，求他不要揭露我的秘密...但我发誓我没杀他！(声音发抖)"), true, _T("scarf"), false, _T("")}
            };

            DialogOption jackOptions[] = {
                {_T(""), _T("那个老顽固！他永远拿我和他失踪的儿子比！说我永远比不上他！财产捐出去也不给我！(愤怒地握紧拳头)"), false, _T(""), false, _T("")},
                {_T(""), _T("我在自己房间...我承认挪用了公款还赌债，但我没想过要杀他！(声音变小)"), false, _T(""), false, _T("")},
                {_T(""), _T("他在撒谎！我是去过书房，但只是想看看遗嘱...我到的时候男爵已经...(突然住口)"), true, _T("footprint"), false, _T("")}
            };

            DialogOption butlerOptions[] = {
                {_T(""), _T("我在厨房准备夜宵，看到杰克先生从书房方向匆匆走出来，神色慌张。(语气坚定)"), false, _T(""), false, _T("")},
                {_T(""), _T("艾米丽需要心脏移植，手术费要五十万...我跪下来求老爷，他却说三十年的忠诚不值钱...(眼眶发红，泪水在打转)"), false, _T(""), false, _T("")},
                {_T(""), _T("这...这不可能！我...(脸色苍白，手开始颤抖)我只是...我只是想...(突然瘫坐在椅子上)"), true, _T("candle"), true, _T("管家袖口的血迹与男爵血型一致！")}
            };

            DialogOption* options = nullptr;
            bool* questionFlags = nullptr;
            bool* q1 = nullptr, *q2 = nullptr, *q3 = nullptr;

            if (currentSuspect == SUSPECT_WIFE) {
                options = wifeOptions;
                q1 = &wifeQuestion1;
                q2 = &wifeQuestion2;
                q3 = &wifeQuestion3;
            } else if (currentSuspect == SUSPECT_JACK) {
                options = jackOptions;
                q1 = &jackQuestion1;
                q2 = &jackQuestion2;
                q3 = &jackQuestion3;
            } else if (currentSuspect == SUSPECT_BUTLER) {
                options = butlerOptions;
                q1 = &butlerQuestion1;
                q2 = &butlerQuestion2;
                q3 = &butlerQuestion3;
            }

            //处理问题是否可以激活
            if (IsPointInRect(x, y, optionX, optionY, optionWidth, optionHeight) && options) {
                if (!*q1) {
                    bool canAsk = !options[0].requiresClue;
                    if (options[0].requiresClue) {
                        if (_tcscmp(options[0].requiredClue, _T("letter")) == 0 && clueLetter) canAsk = true;
                        if (_tcscmp(options[0].requiredClue, _T("scarf")) == 0 && clueScarf) canAsk = true;
                        if (_tcscmp(options[0].requiredClue, _T("candle")) == 0 && clueCandle) canAsk = true;
                        if (_tcscmp(options[0].requiredClue, _T("footprint")) == 0 && clueFootprint) canAsk = true;
                    }
                    if (canAsk) {
                        *q1 = true;
                        PlaySoundEffect(_T("clue"));
                        ShowDialogResponse(options[0].response, options[0].revealsClue, _T(""), options[0].newClue);
                    }
                }
            } else if (IsPointInRect(x, y, optionX, optionY + 60, optionWidth, optionHeight) && options) {
                if (!*q2) {
                    bool canAsk = !options[1].requiresClue;
                    if (options[1].requiresClue) {
                        if (_tcscmp(options[1].requiredClue, _T("letter")) == 0 && clueLetter) canAsk = true;
                        if (_tcscmp(options[1].requiredClue, _T("scarf")) == 0 && clueScarf) canAsk = true;
                        if (_tcscmp(options[1].requiredClue, _T("candle")) == 0 && clueCandle) canAsk = true;
                        if (_tcscmp(options[1].requiredClue, _T("footprint")) == 0 && clueFootprint) canAsk = true;
                    }
                    if (canAsk) {
                        *q2 = true;
                        PlaySoundEffect(_T("clue"));
                        ShowDialogResponse(options[1].response, options[1].revealsClue, _T(""), options[1].newClue);
                    }
                }
            } else if (IsPointInRect(x, y, optionX, optionY + 120, optionWidth, optionHeight) && options) {
                if (!*q3) {
                    bool canAsk = !options[2].requiresClue;
                    if (options[2].requiresClue) {
                        if (_tcscmp(options[2].requiredClue, _T("letter")) == 0 && clueLetter) canAsk = true;
                        if (_tcscmp(options[2].requiredClue, _T("scarf")) == 0 && clueScarf) canAsk = true;
                        if (_tcscmp(options[2].requiredClue, _T("candle")) == 0 && clueCandle) canAsk = true;
                        if (_tcscmp(options[2].requiredClue, _T("footprint")) == 0 && clueFootprint) canAsk = true;
                    }
                    if (canAsk) {
                        *q3 = true;
                        PlaySoundEffect(_T("clue"));
                        ShowDialogResponse(options[2].response, options[2].revealsClue, _T(""), options[2].newClue);
                    }
                }
            } else if (IsPointInRect(x, y, WINDOW_WIDTH - 200, 650, 180, 40)) {
                StartTransition(STATE_GAME);
            }
            break;
        }
        case STATE_ACCUSE://指认真凶
            if (IsPointInRect(x, y, 200, 280, 200, 200)) {
                PlaySoundEffect(_T("wrong"));
                StartTransition(STATE_ENDING_LOSE);
            } else if (IsPointInRect(x, y, 540, 280, 200, 200)) {
                PlaySoundEffect(_T("wrong"));
                StartTransition(STATE_ENDING_LOSE);
            } else if (IsPointInRect(x, y, 880, 280, 200, 200)) {
                if (keyClueCount >= TOTAL_KEY_CLUES) {
                    PlaySoundEffect(_T("win"));
                } else {
                    PlaySoundEffect(_T("wrong"));
                }
                StartTransition(keyClueCount >= TOTAL_KEY_CLUES ? STATE_ENDING_WIN : STATE_ENDING_LOSE);
            } else if (IsPointInRect(x, y, WINDOW_WIDTH - 200, 650, 180, 40)) {
                StartTransition(STATE_GAME);
            }
            break;
        case STATE_ENDING_WIN:

            //可以点击人物查看背景故事
            if (IsPointInRect(x, y, 100, 500, 150, 150)) {
                currentStoryCharacter = 0;
                DrawBackgroundStoryScreen();
            } else if (IsPointInRect(x, y, 280, 500, 150, 150)) {
                currentStoryCharacter = 1;
                DrawBackgroundStoryScreen();
            } else if (IsPointInRect(x, y, 460, 500, 150, 150)) {
                currentStoryCharacter = 2;
                DrawBackgroundStoryScreen();
            } else {
                clueCandle = false;
                clueLetter = false;
                clueScarf = false;
                clueBody = false;
                clueFootprint = false;
                keyClueCount = 0;
                wifeInterrogated = false;
                jackInterrogated = false;
                butlerInterrogated = false;
                wifeQuestion1 = false;
                wifeQuestion2 = false;
                wifeQuestion3 = false;
                jackQuestion1 = false;
                jackQuestion2 = false;
                jackQuestion3 = false;
                butlerQuestion1 = false;
                butlerQuestion2 = false;
                butlerQuestion3 = false;
                StartTransition(STATE_TITLE);//返回标题界面
            }
            break;
        case STATE_ENDING_LOSE:
            //失败重置
            clueCandle = false;
            clueLetter = false;
            clueScarf = false;
            clueBody = false;
            clueFootprint = false;
            keyClueCount = 0;
            wifeInterrogated = false;
            jackInterrogated = false;
            butlerInterrogated = false;
            wifeQuestion1 = false;
            wifeQuestion2 = false;
            wifeQuestion3 = false;
            jackQuestion1 = false;
            jackQuestion2 = false;
            jackQuestion3 = false;
            butlerQuestion1 = false;
            butlerQuestion2 = false;
            butlerQuestion3 = false;
            StartTransition(STATE_TITLE);
            break;
    }
}

//人物背景故事
void DrawBackgroundStoryScreen() {
    cleardevice();
    putimage(0, 0, &imgBackground);

    int boxWidth = 700;
    int boxHeight = 600;
    int boxX = (WINDOW_WIDTH - boxWidth) / 2;
    int boxY = (WINDOW_HEIGHT - boxHeight) / 2;

    setfillcolor(ARGB(200, 20, 20, 30));
    solidroundrect(boxX, boxY, boxX + boxWidth+60, boxY + boxHeight+60, 20, 20);

    setbkmode(TRANSPARENT);
    SetChineseFont(32);
    settextcolor(RGB(255, 220, 100));

    const TCHAR* titles[] = {_T("男爵夫人 - 艾丽丝"), _T("养子杰克"), _T("老管家 - 托马斯")};
    outtextxy(boxX + (boxWidth - textwidth(titles[currentStoryCharacter])) / 2, boxY + 40, titles[currentStoryCharacter]);

    SetChineseFont(22);
    settextcolor(RGB(200, 190, 170));

    int innerY = boxY + 100;
    int lineHeight = 35;

    const TCHAR* wifeStory[] = {
        _T("艾丽丝出生于一个没落的贵族家庭，年轻时是远近闻名的美人。"),
        _T("25岁时，她嫁给了比自己大25岁的老男爵，成为他的第二任妻子。"),
        _T("她与男爵结婚并非出于爱情，而是为了拯救濒临破产的家族。"),
        _T("结婚初期，她确实爱过男爵，但随着时间推移，两人的隔阂越来越深。"),
        _T("三年前，她偷偷资助自己的弟弟做生意，结果弟弟卷款跑路，"),
        _T("留下巨额债务。男爵最近发现了这件事，威胁要揭露她，"),
        _T("并修改遗嘱让她一无所有。她的围巾出现在书房，"),
        _T("是因为她曾去找男爵求情。")
    };

    const TCHAR* jackStory[] = {
        _T("杰克是老男爵的远房侄子，父母早逝后被男爵收养。"),
        _T("他从小就被当作继承人培养，但性格叛逆，与男爵关系紧张。"),
        _T("他其实是男爵亲生儿子的替代品，男爵的亲生儿子在战争中失踪。"),
        _T("他一直活在替代品的阴影下，渴望证明自己。"),
        _T("两年前，他投资失败，欠下了巨额赌债。"),
        _T("他曾伪造男爵签名挪用公款，被男爵发现后威胁要报警。"),
        _T("他声称没去过书房是谎言，他确实去过，但只是想偷改遗嘱。")
    };

    const TCHAR* butlerStory[] = {
        _T("托马斯在古堡工作了30年，亲眼见证了古堡的兴衰。"),
        _T("他是看着杰克长大的，对男爵一家有着深厚的感情。"),
        _T("他的妻子早逝，女儿艾米丽是他唯一的精神寄托。"),
        _T("艾米丽患有罕见的心脏病，需要巨额手术费。"),
        _T("他曾多次向男爵求助，但男爵以家族规矩为由拒绝。"),
        _T("更残酷的是，男爵私下里嘲笑他三十年的忠诚不值钱。"),
        _T("他的动机并非单纯为了钱，更多是被男爵的冷漠逼上绝路。"),
        _T("他栽赃杰克，是因为杰克平时对他态度恶劣，两人积怨已久。")
    };

    const TCHAR** story = nullptr;
    int storyLength = 0;

    if (currentStoryCharacter == 0) {
        story = wifeStory;
        storyLength = 8;
    } else if (currentStoryCharacter == 1) {
        story = jackStory;
        storyLength = 7;
    } else {
        story = butlerStory;
        storyLength = 8;
    }

    for (int i = 0; i < storyLength && innerY < boxY + boxHeight - 100; i++) {
        outtextxy(boxX + 40, innerY, story[i]);
        innerY += lineHeight;
    }

    //跳转提示
    SetChineseFont(20);
    settextcolor(RGB(150, 150, 150));
    outtextxy(boxX + 40, boxY + boxHeight - 60, _T("← 上一位人物"));
    outtextxy(boxX + boxWidth - 150, boxY + boxHeight - 60, _T("下一位人物 →"));

    SetChineseFont(22);
    settextcolor(RGB(255, 255, 200));
    outtextxy(boxX + (boxWidth - 150) / 2, boxY + boxHeight - 30, _T("点击返回结局"));

    FlushBatchDraw();

    bool waiting = true;
    while (waiting) {
        Sleep(30);
        if (MouseHit()) {
            MOUSEMSG msg = GetMouseMsg();
            if (msg.uMsg == WM_LBUTTONDOWN) {
                if (msg.x >= boxX + 40 && msg.x <= boxX + 160 && msg.y >= boxY + boxHeight - 70 && msg.y <= boxY + boxHeight - 40) {
                    currentStoryCharacter = (currentStoryCharacter - 1 + 3) % 3;
                    DrawBackgroundStoryScreen();
                } else if (msg.x >= boxX + boxWidth - 160 && msg.x <= boxX + boxWidth - 30 && msg.y >= boxY + boxHeight - 70 && msg.y <= boxY + boxHeight - 40) {
                    currentStoryCharacter = (currentStoryCharacter + 1) % 3;
                    DrawBackgroundStoryScreen();
                } else {
                    waiting = false;
                }
            }
        }
    }
}

//主函数
int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT);
    LoadImages();
    BeginBatchDraw();
    
    PlayBackgroundMusic();

    //游戏主循环
    while (true) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            StopBackgroundMusic();
            EndBatchDraw();
            closegraph();
            return 0;
        }

        // 如果不在场景过渡中，则绘制当前游戏状态的画面
        if (!isTransitioning) {
            switch (currentState) {
                case STATE_TITLE:
                    DrawTitleScreen();
                    break;
                case STATE_INTRO:
                    DrawIntroScreen();
                    break;
                case STATE_GAME:
                    DrawGameScreen();
                    break;
                case STATE_QUESTION:
                    DrawQuestionScreen();
                    break;
                case STATE_DIALOG:
                    DrawQuestionScreen();
                    break;
                case STATE_ACCUSE:
                    DrawAccuseScreen();
                    break;
                case STATE_ENDING_WIN:
                    DrawEndingScreen(true);
                    break;
                case STATE_ENDING_LOSE:
                    DrawEndingScreen(false);
                    break;
            }
        }
        
        UpdateTransition();
        DrawTransition();

        FlushBatchDraw();
        Sleep(30);

        while (MouseHit()) {
            MOUSEMSG msg = GetMouseMsg();
            if (msg.uMsg == WM_MOUSEMOVE) {
                mouseX = msg.x;
                mouseY = msg.y;
            } else if (msg.uMsg == WM_LBUTTONDOWN) {
                HandleMouseClick(msg.x, msg.y);
            }
        }
    }

    StopBackgroundMusic();
    EndBatchDraw();
    closegraph();
    return 0;
}
