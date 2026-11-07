#!/usr/bin/env python3
"""Deterministic 128x64 monochrome Heltec V4 design; no device claims."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import json, re, shutil, zipfile, math
HERE=Path(__file__).resolve().parent
REPO=HERE.parents[1]
latin=[int(x,16) for x in re.findall(r'0x[0-9a-fA-F]+',(REPO/'src/helpers/ui/Glcd5x7.inc').read_text())]
src=(REPO/'src/helpers/ui/CyrillicGlyphs.inc').read_text()
def array(name):
    return [int(x,16) for x in re.findall(r'0x[0-9a-fA-F]+',re.search(r'\b'+name+r'\[\d+\] PROGMEM = \{(.*?)\};',src,re.S).group(1))]
adv,off,bits=map(array,['CYR_OLED_ADV','CYR_OLED_OFF','CYR_OLED_BITS'])
def text(im,x,y,s,scale=1,color=1):
    assert 0<=x and x+6*len(s)*scale<=128,(s,x,scale)
    d=ImageDraw.Draw(im)
    for c in s:
        cp=ord(c)
        if 32<=cp<=127:
            glyph=latin[(cp-32)*5:(cp-31)*5]
            pts=((col,row) for col,v in enumerate(glyph) for row in range(8) if v&(1<<row))
        else:
            idx=cp-0x410 if 0x410<=cp<=0x42f else 32 if cp==0x401 else cp-0x430+33 if 0x430<=cp<=0x44f else 65 if cp==0x451 else None
            assert idx is not None,(c,s)
            pts=((col,row-1) for row in range(10) for col in range(adv[idx]) if bits[off[idx]+row]&(0x80>>col))
        for dx,dy in pts:
            xx,yy=x+dx*scale,y+dy*scale
            assert 0<=xx and xx+scale<=128 and 0<=yy and yy+scale<=64,(s,xx,yy)
            d.rectangle((xx,yy,xx+scale-1,yy+scale-1),fill=color)
        x+=6*scale

def header(im,title,root=False,pct='83%',clock='10:14'):
    d=ImageDraw.Draw(im);d.rectangle((0,0,127,11),fill=1)
    name=title if root else './'+title
    maxlen=10 if root else (16 if len(pct)<=3 else 15)
    if len(name)>maxlen:name=name[:maxlen-2]+'..'
    text(im,2,2,name,color=0)
    if root:text(im,68,2,clock,color=0)
    text(im,128-len(pct)*6,2,pct,color=0)

def base(title,rows,root=False,selected=None,pct='83%',clock='10:14'):
    im=Image.new('1',(128,64),0); header(im,title,root,pct,clock); d=ImageDraw.Draw(im)
    assert len(rows)<=4,(title,rows)
    for i,row in enumerate(rows):
        assert len(row)<=20,(title,row)
        y=16+i*12
        if i==selected:
            d.rectangle((1,y-1,126,y+9),fill=1)
            text(im,4,y,row,color=0)
        else:text(im,4,y,row)
    return im

items=[]
def add(section,key,label,title,rows=(),root=False,selected=None,note='',kind='',pct='83%',clock='10:14'):
    assert all(x['key']!=key for x in items),key
    im=base(title,rows,root,selected,pct,clock)
    items.append(dict(section=section,key=key,label=label,title=title,rows=list(rows),root=root,selected=selected,note=note,kind=kind,image=im))
    return im

def menu(section,key,label,title,rows,selected=0,note=''):
    return add(section,key,label,title,rows,selected=selected,note=note)

S='01 · Основной круг'
add(S,'home','Главная','FDSKY',['BLE подключён','Очередь телефона  3','Сообщений в памяти 5','05.11.2026  ЧТ'],root=True)
add(S,'messages','Сообщения','Сообщения',['В памяти           5','На телефон         3','Непрочитанных      2','Последнее: 3 мин'],root=True,note='Локальная память и очередь телефона — разные счётчики.')
add(S,'links','Связь','Связь',['BLE  вкл   клиент','WiFi вкл   нет','USB  вкл   ?','IP 192.168.1.42'],root=True,note='USB ? — без детектора наличия клиента.')
add(S,'network','Сеть','Сеть',['Соседи: 4','South           +9','Hill-2          +5','Мост            -3'],root=True,note='Значения соседей — SNR, не RSSI.')
r=add(S,'radio','Радио · приём','Радио',root=True,kind='radio',note='Крупная частота, обведённый ряд параметров и NF как в референсе; обновление оценки примерно 2 с.')
text(r,12,17,'868.731',scale=2);text(r,102,25,'MHz');d=ImageDraw.Draw(r);d.rectangle((1,36,126,47),outline=1);text(r,4,38,'BW62.5 SF7 CR5 TX22');text(r,3,53,'NF -111 dBm');d.rectangle((79,51,126,61),outline=1)
for i,h in enumerate([2,3,3,4,3,5,4,6,4,5,4]):d.rectangle((81+i*4,60-h,83+i*4,59),fill=1)
g=add(S,'gps','GPS · фикс','GPS',root=True,kind='gps',note='Расположение как в WhisperOS: спутники и шкала, высота и скорость, координаты. Скорость требует нового источника данных.')
text(g,3,16,'SAT 8');d=ImageDraw.Draw(g);d.rectangle((44,15,126,24),outline=1);d.rectangle((46,17,98,22),fill=1)
text(g,3,28,'ALT 62м    --.-км/ч');text(g,3,40,'LAT  55.755800');text(g,3,52,'LON  37.617600')
add(S,'beacon','Маяк','Маяк',['Группа     Squad','Интервал   30 мин','GPS        фикс есть','Отправлен  4 мин'],root=True,note='Отправлено не означает доставлено.')
add(S,'freq','Частоты · обзор','Частоты',['Сеть    868.731 MHz','Поиск шума','Доступен по команде','Радио: приём'],root=True,note='Сканирование запускается отдельно с предупреждением о временном прерывании приёма.')
add(S,'recent','Недавние','Недавние',['Mytischi-4      12с','CAO_Tazanka      4м','Zelenograd      23м','MO_Klyazma       1ч'],root=True)
add(S,'noise','Шум','Шум',['RSSI       -109 dBm','NF         -111 dBm','Пик         -98 dBm','Приём пакета: нет'],root=True,note='Текущий RSSI, шумовой фон NF и пик — разные значения.')
add(S,'sensors','Датчики','Датчики',['Напряж.      4.03 В','Температура  22.4 C','Влажность       48%','Давление  1013 hPa'],root=True,note='Внешние датчики выводятся только при наличии и поддержке сборки.')
add(S,'battery','Батарея','Батарея',['Напряжение  4.030 В','Оценка заряда   83%','ADC           5.420','Источник: неизвестен'],root=True,note='Процент вычислен по напряжению; статус источника только при достоверном детекторе.')
add(S,'power','Питание','Питание',['Режим       Eco max','Сон CPU     нет','RX Boost    вкл','LNA         выкл'],root=True,note='Показаны фактические состояния; LNA только если аппаратно управляем.')
add(S,'about','О прошивке','Сведения',['MeshCore Companion','v1.18.0-0.2.0','Heltec V4 OLED','by FDSKY'],root=True,note='Номер версии пример; на заставке подпись by FDSKY.')
add(S,'settings','Настройки · вход','Настройки',['Удержание — открыть'],root=True,note='Применяется стандартный символ кириллицы; на устройстве длинное тире рисуется линией.') if False else add(S,'settings','Настройки · вход','Настройки',['Радио и приём','Связь и питание','GPS, маяк, экран','Время и система'],root=True)

S='02 · Состояния основных окон'
for key,label,title,rows,note in [
('home_off','Главная · без клиента','FDSKY',['Клиент не подключён','Очередь телефона  0','Сообщений в памяти 0','Время не задано'],''),
('msg_empty','Сообщений нет','Сообщения',['Сообщений пока нет','', 'Приём работает'],''),
('links_wait','Wi-Fi подключается','Связь',['BLE  вкл   нет','WiFi ждёт сеть','USB  вкл   ?','IP --'],''),
('network_empty','Нет соседей','Сеть',['Соседей пока нет','', 'Опрос из действий'],'Нет полученных ответов не означает, что сеть пуста.'),
('radio_wait','Радио · нет оценки NF','Радио',['868.731 MHz','BW62.5 SF7 CR5 TX22','NF --','Данных пока нет'],'NF -- до первой оценки.'),
('gps_wait','GPS · поиск','GPS',['SAT 3   поиск','ALT --       -- км/ч','LAT  --','LON  --'],'Без fix не показывать сохранённые координаты как текущие.'),
('gps_off','GPS · выключен','GPS',['Модуль выключен','SAT --','LAT  --','LON  --'],''),
('gps_missing','GPS · нет модуля','GPS',['GPS недоступен','Проверьте провода','SAT --','LAT/LON --'],''),
('beacon_off','Маяк · выключен','Маяк',['Статус: выключен','Отправки нет','GPS: выкл',''],''),
('beacon_wait','Маяк · ожидание GPS','Маяк',['Группа     Squad','Интервал   30 мин','Нет фикса GPS','Отправка отложена'],''),
('freq_run','Частоты · сканирование','Частоты',['Сканирование   7/17','868.775 MHz','Шум      -114 dBm','Отмена: 2 клика'],'После отмены и завершения исходная частота восстанавливается.'),
('freq_result','Частоты · результат','Частоты',['868.481    -121 dBm','868.606    -119 dBm','868.731*   -112 dBm','* частота сети'],'Частота автоматически не меняется.'),
('recent_empty','Нет объявлений','Недавние',['Объявлений пока нет','', '', ''],''),
('sensors_empty','Датчики · нет данных','Датчики',['Нет внешних данных','', '', ''],''),
('battery_unknown','Батарея · нет измерения','Батарея',['Напряжение: --','Заряд: --%','ADC недоступен',''],''),
('power_blocked','Питание · сон занят','Питание',['Режим       Custom','Сон: недоступен','Причина: BLE','RX Boost: вкл'],'Сон отображается как возможность, а не как выключенный CPU.')]:
    im=add(S,key,label,title,rows,root=True,note=note,pct='--%' if key=='battery_unknown' else '83%')
    if key == 'radio_wait':
        im=Image.new('1',(128,64),0); header(im,'Радио',True)
        text(im,12,17,'868.731',scale=2); text(im,102,25,'MHz')
        d=ImageDraw.Draw(im); d.rectangle((1,36,126,47),outline=1)
        text(im,4,38,'BW62.5 SF7 CR5 TX22'); text(im,3,53,'NF --')
        d.rectangle((79,51,126,61),outline=1); items[-1]['image']=im
    if key == 'gps_wait':
        im=Image.new('1',(128,64),0); header(im,'GPS',True)
        text(im,3,16,'SAT 3'); d=ImageDraw.Draw(im); d.rectangle((44,15,126,24),outline=1)
        d.rectangle((46,17,64,22),fill=1); text(im,3,28,'ALT --     --.-км/ч')
        text(im,3,40,'LAT  --'); text(im,3,52,'LON  --'); items[-1]['image']=im

S='03 · Сообщения и сеть'
menu(S,'msg_list','Сохранённые сообщения','Сообщения',['Алексей       3 мин','Группа        5 мин','Лиса         12 мин'],0)
add(S,'msg_text1','Сообщение · первая часть','Алексей',['Привет! Встречаемся','у старого моста','в 18:00. Возьми','1/2'])
add(S,'msg_text2','Сообщение · продолжение','Алексей',['рацию и запасной','аккумулятор.','До встречи!','2/2'])
menu(S,'msg_actions','Действия сообщения','Сообщение',['Следующее','Отметить прочитанным','Информация','Удалить из просмотра'])
add(S,'msg_info','Сведения о сообщении','Сообщение',['Личное: Алексей','Получено: 3 мин','Ещё в очереди: 1'])
menu(S,'msg_clear','Подтвердить очистку','Очистка',['Отмена','Очистить просмотр'],0,note='Локальный просмотр и очередь клиента не объединять.')
menu(S,'net_actions','Действия сети','Сеть',['Опросить соседей','Объявить себя','Назад'])
menu(S,'recent_actions','Действия объявлений','Недавние',['Объявить себя','Назад'])
menu(S,'noise_actions','Действия шума','Шум',['Сбросить пик','Назад'])
add(S,'noise_reset','Пик шума сброшен','Шум',['Пик сброшен','Новые измерения','', ''])
add(S,'sensors_refresh','Датчики обновлены','Датчики',['Опрос выполнен','', '', ''])
add(S,'polling','Опрос соседей','Сеть',['Запрос отправлен','Ожидаем ответы','', ''])
add(S,'poll_error','Ошибка опроса','Сеть',['Запрос не отправлен','Попробуйте позже'])
add(S,'advert_sent','Объявление отправлено','Недавние',['Отправлено','', ''])
add(S,'advert_error','Ошибка объявления','Недавние',['Не отправлено','Попробуйте позже'])

S='04 · Настройки · меню'
menu(S,'settings_a','Настройки · начало','Настройки',['Радио','Интерфейсы','Питание','Батарея'])
menu(S,'settings_b','Настройки · продолжение','Настройки',['GPS','Маяк','Дисплей','Время'])
menu(S,'settings_c','Настройки · система','Настройки',['Язык','Звук','Выключение','Сведения'],note='Звук — только при наличии buzzer.')
menu(S,'radio_settings','Радио · действия','Радио',['Профиль','RX Boost: вкл','LNA: выкл',''],note='LNA показывать только при поддержке управления FEM.')
menu(S,'profiles','Выбор радиопрофиля','Профиль',['Normal','Long','HiNoise'],1)
add(S,'profile_normal','Normal · предпросмотр','Normal',['868.731 BW62.5','SF7  CR5  TX22','Повтор: выкл','Проверить параметры'],note='Normal восстанавливает сохранённую сеть или параметры сборки.')
add(S,'profile_long','Long · предпросмотр','Long',['869.495 BW62.5','SF11 CR5  TX22','Повтор: вкл','Проверить параметры'],note='TX не задаётся профилем. Repeat зависит от допустимости частоты в коде.')
add(S,'profile_noise','HiNoise · предпросмотр','HiNoise',['869.495 BW31.25','SF10 CR7  TX22','Повтор: вкл','Проверить параметры'])
menu(S,'profile_confirm','Подтвердить профиль','Профиль',['Применить Long?','Отмена','Применить'],1)
add(S,'profile_applied','Профиль применён','Профиль',['Профиль: Long','Радио обновлено','Повтор: вкл',''])
menu(S,'lna_on','LNA · настройка','LNA',['Сейчас: выкл','Включить','Оставить выкл'],2)
add(S,'lna_na','LNA · недоступен','LNA',['Управление FEM','недоступно','RX Boost отдельно',''])
menu(S,'rx_boost','RX Boost · настройка','RX Boost',['Сейчас: вкл','Выключить','Оставить вкл'],2)

S='05 · Настройки · связь и питание'
menu(S,'interfaces','Интерфейсы','Интерфейсы',['BLE      вкл','Wi-Fi    вкл','USB      вкл',''],0)
add(S,'ble','Bluetooth · состояние','BLE',['Статус: вкл','Клиент: подключён','PIN: 123456',''])
add(S,'wifi','Wi-Fi · состояние','Wi-Fi',['SSID: Home','IP: 192.168.1.42','5000: подключён','5001: нет'],note='Порт не определяет личность клиента.')
add(S,'wifi_wait','Wi-Fi · нет сети','Wi-Fi',['SSID: Home','Подключение...','IP: --','5000/5001: --'])
add(S,'usb','USB · состояние','USB',['Интерфейс: вкл','Клиент: неизвестно','', ''])
menu(S,'interface_confirm','Отключить интерфейс','Wi-Fi',['Выключить Wi-Fi?','Отмена','Выключить'],1,note='Отмена выбрана по умолчанию, чтобы не оборвать соединение случайно.')
menu(S,'power_modes','Режимы питания','Питание',['Eco max','Eco high','Eco good','Другие режимы'])
for i,(sleep,rx,lna) in enumerate([('да','выкл','выкл'),('да','вкл','выкл'),('да','вкл','вкл'),('нет','выкл','выкл'),('нет','вкл','выкл'),('нет','вкл','вкл')],1):
    add(S,f'power_mode{i}',f'Питание · вариант {i}',f'Режим {i}',[f'Сон CPU: {sleep}',f'RX Boost: {rx}',f'LNA: {lna}','Предпросмотр'],note='Варианты зависят от платы и ограничений сна.')
menu(S,'power_confirm','Подтвердить питание','Питание',['Применить режим?','Отмена','Применить'],1)

S='06 · Аккумулятор и ADC'
menu(S,'adc_menu','Калибровка ADC','ADC',['Полный заряд 4.20 В','По мультиметру','Сброс коэффициента'])
add(S,'adc_full','Условия калибровки','Полный заряд',['Полностью заряжен?','Эталон: 4.200 В','Только 1S Li-ion',''],note='USB не означает полный заряд.')
add(S,'adc_edit','Ввод мультиметра','Мультиметр',['Эталон: 4.150 В','           ^','Далее: удержание','Отмена на проверке'],note='Клик увеличивает цифру, два клика уменьшают, удержание выбирает следующую; после последней — подтверждение.')
menu(S,'adc_confirm','Подтвердить ADC','ADC',['4.030 > 4.150 В','Отмена','Применить'],1)
menu(S,'adc_reset','Сбросить ADC','Сброс ADC',['Вернуть ADC платы?','Отмена','Сбросить'],1)
add(S,'adc_done','ADC · сохранено','ADC',['Калибровка записана','ADC: 5.581','Напряжение: 4.150 В',''])
add(S,'adc_error','ADC · ошибка','ADC',['Не сохранено','Значение не изменено','Попробуйте снова',''])
add(S,'battery_low','Низкий заряд','Батарея',['Низкое напряжение','Подключите питание','', ''],pct='3%')

S='07 · GPS, маяк и частоты'
menu(S,'gps_settings','Настройка GPS','GPS',['GPS: включён','Выключить GPS','Назад'])
add(S,'gps_powered','GPS без отключения питания','GPS',['GPS логически выкл','Питание осталось','', ''],note='Только если плата не умеет физически отключать питание модуля.')
menu(S,'beacon_settings','Настройки маяка','Маяк',['Режим','Получатель','Интервал','Остановить'])
menu(S,'beacon_modes','Режимы маяка · 1','Режим маяка',['Выключен','Локальный advert','Flood advert','Группа'])
menu(S,'beacon_modes2','Режимы маяка · 2','Режим маяка',['Flood advert','Группа','Личное сообщение'],1)
menu(S,'beacon_target','Получатель маяка','Получатель',['Squad North','Общий канал','Алексей'])
add(S,'beacon_no_target','Нет получателя','Маяк',['Нет получателя','Добавьте контакт','или канал в app',''])
menu(S,'beacon_interval1','Интервал маяка · 1','Интервал',['5 мин','10 мин','15 мин','30 мин'])
menu(S,'beacon_interval2','Интервал маяка · 2','Интервал',['15 мин','30 мин','60 мин','120 мин'],1)
menu(S,'beacon_stop','Выключить маяк','Маяк',['Остановить маяк?','Отмена','Остановить'],1)
menu(S,'scan_confirm','Начать поиск частоты','Частоты',['Приём прервётся','Отмена','Начать поиск'],1,note='Это исследование шума; не переключать сеть автоматически.')
add(S,'scan_cancel','Поиск частоты отменён','Частоты',['Поиск отменён','Радио восстановлено','868.731 MHz',''])
menu(S,'scan_actions','Действия поиска','Частоты',['Повторить поиск','Закрыть результат'])

S='08 · Дисплей, время и система'
menu(S,'display','Настройки дисплея','Дисплей',['Часы: вкл','Таймаут: 30 с','Язык: русский',''])
menu(S,'screen_timeout','Таймаут экрана','Таймаут',['Никогда','15 с','30 с','60 с'],2)
menu(S,'screen_timeout2','Таймаут · продолжение','Таймаут',['30 с','60 с','120 с'],1)
menu(S,'language','Язык интерфейса','Язык',['Русский','English'])
menu(S,'clock_actions','Настройки времени','Время',['Запросить время','Узлы времени','Часовой пояс',''])
menu(S,'clock_nodes','Узлы времени','Узлы времени',['Добавить узел','Запросить время','Очистить список',''])
menu(S,'node_picker','Выбрать узел времени','Выбор узла',['South','Hill-2','Base',''])
add(S,'node_empty','Нет контактов','Выбор узла',['Контактов пока нет','Добавьте в app','', ''])
menu(S,'clock_clear','Очистить узлы времени','Узлы времени',['Очистить список?','Отмена','Очистить'],1)
add(S,'clock_request','Запрос времени','Время',['Запрос отправлен','Ожидаем ответ','', ''])
add(S,'clock_missing','Нет источника времени','Время',['Некому отправить','Добавьте узел','', ''])
add(S,'timezone','Часовой пояс','Часовой пояс',['UTC +03:00','Изменение через CLI','set tz +3',''])
menu(S,'sound','Звук','Звук',['Включён','Выключен'],0,note='Показывать только если есть buzzer.')
menu(S,'shutdown','Выключение','Выключение',['Выключить питание?','Отмена','Выключить'],1)
add(S,'shutdown_run','Выключение выполняется','Выключение',['Выключаю питание','', '', ''])
add(S,'error','Ошибка действия','Настройка',['Не удалось применить','Значение не изменено','Попробуйте снова',''])
S='09 · Загрузка и системные состояния'
im=add(S,'boot','Заставка','MeshCore',['Companion','v1.18.0-0.2.0','by FDSKY',''])
im=Image.new('1',(128,64),0); text(im,16,10,'MeshCore',scale=2); text(im,37,36,'Companion'); text(im,40,51,'by FDSKY'); items[-1]['image']=im
im=add(S,'display_off','Экран выключен','',['','','','']);items[-1]['image']=Image.new('1',(128,64),0)
im=add(S,'cli_rescue','CLI rescue','CLI rescue',['Подключите USB','Служебная консоль','', ''],note='Доступ через удержание во время загрузки сохраняется.')
im=Image.new('1',(128,64),0); text(im,4,10,'CLI rescue'); text(im,4,30,'Подключите USB'); text(im,4,44,'Служебная консоль'); items[-1]['image']=im

# V4: these information screens are reached from Settings, not the root cycle.
moved = {'battery': '06 · Аккумулятор и ADC',
         'battery_unknown': '06 · Аккумулятор и ADC',
         'power': '05 · Настройки · связь и питание',
         'power_blocked': '05 · Настройки · связь и питание',
         'about': '08 · Дисплей, время и система'}
for s in items:
    if s['key'] in moved:
        s['section'] = moved[s['key']]
        s['root'] = False
        s['image'] = base(s['title'],s['rows'],False,s['selected'],
                          '--%' if s['key']=='battery_unknown' else '83%')
# Stable order within each section, independent of declaration order.
items.sort(key=lambda s: s['section'])

# Gallery and archive. The pixels are derived from the firmware font; these are proposed states.
for directory in ('png-1x','png-4x'):
    p=HERE/directory
    if p.exists():shutil.rmtree(p)
    p.mkdir()
for i,s in enumerate(items,1):
    s['number']=i;s['file']=f'{i:03d}_{s["key"]}.png'
    s['image'].save(HERE/'png-1x'/s['file'])
    s['image'].resize((512,256),Image.Resampling.NEAREST).save(HERE/'png-4x'/s['file'])
manifest=[{k:v for k,v in s.items() if k!='image'} for s in items]
(HERE/'screens.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
sections=list(dict.fromkeys(s['section'] for s in items))
regular='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf';bold='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
font_title=ImageFont.truetype(bold,28);font_label=ImageFont.truetype(bold,17);font_sub=ImageFont.truetype(regular,15)
for j,section in enumerate(sections,1):
    subset=[s for s in items if s['section']==section]; cols=3; rows=math.ceil(len(subset)/cols); w=1280; h=108+rows*270
    board=Image.new('RGB',(w,h),'#eef2ed'); d=ImageDraw.Draw(board)
    d.text((25,20),section+'  ·  V4',font=font_title,fill='#17332a')
    d.text((25,63),'128×64 · шрифт прошивки · ×3 · данные на экранах приведены для примера',font=font_sub,fill='#4d665a')
    for i,s in enumerate(subset):
        x=25+(i%cols)*420;y=105+(i//cols)*270
        d.text((x,y),f'{s["number"]:03d}  {s["label"]}',font=font_label,fill='#18362a')
        board.paste(s['image'].resize((384,192),Image.Resampling.NEAREST).convert('RGB'),(x,y+28))
    board.save(HERE/f'{j:02d}-sheet.png',optimize=True)
with zipfile.ZipFile(HERE/'screens-v4.zip','w',zipfile.ZIP_DEFLATED,9) as z:
    for folder in ('png-1x','png-4x'):
        for file in sorted((HERE/folder).glob('*.png')):z.write(file,file.relative_to(HERE))
    z.write(HERE/'screens.json','screens.json')
print(f'{len(items)} screens; {len(sections)} sheets; 128x64 bounding assertions passed')
