#include <iostream>
#include <pqxx/pqxx>
#include <tgbot/tgbot.h>
#include "date.h"
#include <sstream>
#define FLIGTHS_IN_ONE_MESSAGE 2
#define TIMEZONE 7
#define SECONDS_IN_HOUR 3600
#define SECONDS_IN_DAY 86400
using namespace std;
using namespace pqxx;
using namespace TgBot;
using namespace date;
using namespace chrono;

string outputFligthRowString (row flightRow) {
  string messageToSend;
messageToSend += string(flightRow[9].c_str()) + " - ";
messageToSend += string(flightRow[11].c_str()) + ", ";
messageToSend += "spent " + string(flightRow[3].c_str()) + " mAh, ";
int seconds = stoi(string(flightRow[4].c_str()));
messageToSend += to_string(seconds / 60) + ":"
+ to_string(seconds % 60) + "\n";
return messageToSend;
}

int getDayInt(field field) {
  return(stoi(string(field.c_str()))+TIMEZONE*SECONDS_IN_HOUR)/SECONDS_IN_DAY;
}


string token, commandsList, connInfo, currentBatteryID,
    currentSpentEnergy, currentFlightType;

bool waitingForBatteryType = false, waitingForBattery = false,
    waitingForFlightType = false, waitingForSpentEnergy = false,
    waitingForTime = false, waitingForBatteryRemoving = false,
    waitingForShowingNextFlights = false;

string outputBatteryTypeString (row row, bool shorter_form = false) {
  string res;

  res += string(row["manufacturer_name"].c_str()) + " ";

  if (!string(row["type_name"].c_str()).empty())
    res += string(row["type_name"].c_str()) + " ";

  if (!string(row["capacity"].c_str()).empty())
    res += string(row["capacity"].c_str()) + " mAh ";

  if (!string(row["max_voltage"].c_str()).empty())
    res += "max: "
        + to_string(
            ((double) stoi(string(row["max_voltage"].c_str())))
                / 100).substr(0, 4) + "V ";

  if (!shorter_form) {

    if (!string(row["max_current"].c_str()).empty())
      res += string(row["max_current"].c_str())  + " ";

  if (!string(row["nominal_voltage"].c_str()).empty())
    res += "nominal: "
        + to_string(
            ((double) stoi(string(row["nominal_voltage"].c_str())))
                / 100).substr(0, 4) + "V ";

    if (!string(row["watthours"].c_str()).empty())
      res += "nominal: "
          + to_string(
              ((double) stoi(string(row["watthours"].c_str())))
                  / 100).substr(0, 4) + "V ";
  }
  res += "\n";
  return res;

}

pqxx::result* flight;
uint64_t currentDay = 0; int currentFlightRow = 0, currentTen = 0;
pqxx::connection *C;
int main(int argc, char *argv[]) {

  ifstream tokenFile;
  tokenFile.open("token.txt");
  getline(tokenFile, token);

  std::ifstream commandsListStream("commandsList.txt");
  string temp;
  while (std::getline(commandsListStream, temp)) {
    commandsList += temp + "\n";
  }

  std::ifstream connectionFile("conninfo.txt");

  getline(connectionFile, connInfo);

  TgBot::Bot bot(token);
  C = new pqxx::connection(connInfo);
  std::cout << "Connected to " << C->dbname() << '\n';

  bot.getEvents().onCommand("start", [&bot](TgBot::Message::Ptr message) {
    //searching for user who sent message in db

    work tx(*C);
    pqxx::result r = tx.exec("SELECT * FROM bot_user WHERE id = " + to_string(message->chat->id));
    if (r.size() == 0) {
      tx.exec(
          "insert into bot_user (id, nickname) values("
              + to_string(message->chat->id) + ", '"
              + to_string(message->from->firstName + "')"));
      tx.commit();

      bot.getApi().sendMessage(message->chat->id,
                               "Wassupo, " + to_string(message->from->firstName) + "\n\n"
                                   + commandsList);
    } else {
      bot.getApi().sendMessage(message->chat->id,
                               "Wassupo, " + to_string(message->from->firstName)
                                   + "\nThis bot is created to manage batteries for quadrocopters.\n"
                                     "List of commands:\n"
                                   + commandsList);
    }

  });

  bot.getEvents().onCommand("help", [&bot](TgBot::Message::Ptr message) {
    bot.getApi().sendMessage(message->chat->id, commandsList);
  });

  bot.getEvents().onCommand("add_battery",
                            [&bot](TgBot::Message::Ptr message) {
                              string messageToSend =
                                  "Select a battery type (send a number): \n";
                              pqxx::work tx(*C);
                              pqxx::result battery_type = tx.exec(
                                  "SELECT battery_type.name as type_name, battery_manufacturer.name "
                                  "as manufacturer_name, *"
                                  " FROM battery_type left join battery_manufacturer on "
                                  "battery_type.manufacturer_id = battery_manufacturer.id");
                              int i = 0;
                              for (auto row : battery_type) {
                                i++;

                                messageToSend += to_string(i) + ") " + outputBatteryTypeString(row) ;
                              }
                              bot.getApi().sendMessage(message->chat->id, messageToSend);
                              waitingForBatteryType = true;
                              tx.commit();
                            });

  bot.getEvents().onCommand("my_batteries",
                            [&bot](const TgBot::Message::Ptr message) {
                              string messageToSend = "List of your batteries: \n";
                              pqxx::work tx(*C);
                              pqxx::result batteryOfUser =
                                  tx.exec("select battery_manufacturer.name as manufacturer_name, * "
                                          "from (SELECT battery_type.name as type_name, * from battery "
                                          "left join battery_type on battery.battery_type_id = battery_type.id) "
                                          "as t left join  battery_manufacturer on t.manufacturer_id = "
                                          "battery_manufacturer.id where user_id = " + to_string(message->chat->id) +
                                          "order by special_bat_id");
                                                                                                                     ;

                              if (batteryOfUser.size() == 0) {
                                messageToSend +=
                                    "You don't have any batteries (add some with command /add_battery)\n";
                              } else {
                                int i = 0;

                                for (auto row : batteryOfUser) {
                                  i++;
                                  messageToSend += to_string(i) + ") " + string(row["special_bat_id"].c_str()) +
                                     " - " + outputBatteryTypeString(row, true);
                                }
                              }
                              bot.getApi().sendMessage(message->chat->id, messageToSend);
                            });

  bot.getEvents().onCommand("add_flight",
                            [&bot](TgBot::Message::Ptr message) {
                              string messageToSend =
                                  "Choose the battery from your list (send ID, for example, G1):\n";
                              pqxx::work tx(*C);
                              pqxx::result batteryOfUser =
                                  tx.exec("select battery_manufacturer.name as manufacturer_name, * "
                                          "from (SELECT battery_type.name as type_name, * from battery "
                                          "left join battery_type on battery.battery_type_id = battery_type.id) "
                                          "as t left join  battery_manufacturer on t.manufacturer_id = "
                                          "battery_manufacturer.id where user_id = " +
                                          to_string(message->chat->id) +
                                          "order by special_bat_id");
                              if (batteryOfUser.size() == 0) {
                                messageToSend +=
                                    "You don't have any batteries :(\nAdd some with command /add_battery";
                              } else {
                                int i = 0;
                                for (auto row : batteryOfUser) {
                                  i++;


                              }
                              bot.getApi().sendMessage(message->chat->id, messageToSend);
                              waitingForBattery = true;
                              }
                            });

  bot.getEvents().onCommand("my_flights",
                            [&bot](TgBot::Message::Ptr message) {
                              string messageToSend = "All your flights:\n";
                              pqxx::work tx(*C);
                              flight =
                                  new result(tx.exec(
                                      "select * from flight left join battery on flight.battery_id = battery.id \
                                              left join flight_type on flight.flight_type_id = flight_type.id \
                                      where user_id =" + to_string(message->chat->id) + "order by timestamp desc"));



                              if (flight->size() == 0) {
                                bot.getApi().sendMessage(message->chat->id,
                                                         "You haven't any flights (add_some - /add_flight)");
                              } else {


                                currentTen++;
                                for (int i = currentFlightRow; i !=
                                min(currentTen*FLIGTHS_IN_ONE_MESSAGE, flight->size()); i++) {
                                  currentFlightRow++;
                                row flightRow = (*flight)[i];
                                if (currentDay == 0 ||
                                    getDayInt(flightRow["timestamp"]) != currentDay) {
                                  currentDay = getDayInt(flightRow["timestamp"]);
                                  ostringstream dateStream;
                                  dateStream << sys_days {days(currentDay)};
                                  messageToSend += dateStream.str() + "\n";
                                }
                                messageToSend += outputFligthRowString(flightRow);
                                }
                              }
                              bot.getApi().sendMessage(message->chat->id, messageToSend);
                              if (currentFlightRow < flight->size()) {
                                bot.getApi().sendMessage(message->chat->id, "More? (send one letter y/n)");
                              waitingForShowingNextFlights = true;
                              }
  });

  bot.getEvents().onCommand("flights_of_battery",
                            [&bot](TgBot::Message::Ptr message) {
                              string batID = message->text.substr(message->text.length() - 2, 2);
                              string messageToSend = "All flights with battery " + batID + ":\n";
                              pqxx::work tx(*C);
                              flight =
                                  new result(tx.exec(
                                      "select * from flight left join battery on flight.battery_id = battery.id \
                                              left join flight_type on flight.flight_type_id = flight_type.id \
                                      where user_id =" + to_string(message->chat->id)
                                          + " and special_bat_id = '" + batID + "' order by timestamp desc"));
                              if (flight->size() == 0) {
                                sys_days ();
                                bot.getApi().sendMessage(message->chat->id, "You haven't any flights with this battery (add_some - /add_flight)");
                              } else {

                                if (flight->size() == 0) {
                                  bot.getApi().sendMessage(message->chat->id,
                                                           "You haven't any flights (add_some - /add_flight)");
                                } else {

                                  int remaining =  flight->size() - currentFlightRow;
                                  currentTen++;
                                  for (int i = currentFlightRow; i !=
                                      min(currentTen*FLIGTHS_IN_ONE_MESSAGE, remaining); i++) {
                                    currentFlightRow++;
                                    row flightRow = (*flight)[i];
                                    if (currentDay == 0 ||
                                        getDayInt(flightRow["timestamp"]) != currentDay) {
                                      currentDay = getDayInt(flightRow["timestamp"]);
                                      ostringstream dateStream;
                                      dateStream << sys_days {days(currentDay)};
                                      messageToSend += dateStream.str() + "\n";
                                    }
                                    messageToSend += outputFligthRowString(flightRow);
                                  }
                                }
                                bot.getApi().sendMessage(message->chat->id, messageToSend);
                                if (currentFlightRow < flight->size()) {
                                  bot.getApi().sendMessage(message->chat->id, "More? (send one letter y/n)");
                                  waitingForShowingNextFlights = true;}}
                              
                            });





  bot.getEvents().onCommand("remove_battery",
                            [&bot](TgBot::Message::Ptr message) {

                              pqxx::work tx(*C);
                              pqxx::result batteryOfUser =
                                  tx.exec(
                                      "SELECT battery_type_id, special_bat_id \
									FROM battery where user_id = "
                                          + to_string(message->chat->id)
                                          + " order by special_bat_id");
                              if (batteryOfUser.size() == 0) {
                                bot.getApi().sendMessage(message->chat->id,
                                    "You don't have any batteries :(\nAdd some with command /add_battery");
                              } else {
                                tx.exec("delete from battery where user_id = " + to_string(message->chat->id) + " and special_bat_id = '"
                                            + to_string(message->text) + "'");
                                bot.getApi().sendMessage(message->chat->id,
                                                         "Battery " + to_string(message->text)
                                                             + " was removed from your list. Check - /my_batteries");
                                waitingForBatteryRemoving = false;
                                tx.commit();
                              }
                            });

  bot.getEvents().onAnyMessage([&bot](TgBot::Message::Ptr message) {
    if (waitingForBatteryType) {
      pqxx::work tx(*C);
      pqxx::result r = tx.exec("select battery_manufacturer.name as manufacturer_name, * "
                               "from (SELECT battery_type.name as type_name, * from battery "
                               "left join battery_type on battery.battery_type_id = battery_type.id) "
                               "as t left join  battery_manufacturer on t.manufacturer_id = "
                               " battery_manufacturer.id where user_id = " + to_string(message->chat->id)
                                   + " AND battery_type_id = "
                                   + to_string(message->text) + " order by special_bat_id desc");


      string bat_id = string(r[0]["special_bat_id"].c_str())[0] +
          to_string(stoi(string(r[0]["special_bat_id"].c_str()).substr(1)) + 1) ;
      tx.exec(
          "insert into battery (user_id, battery_type_id, special_bat_id) values("
              + to_string(message->chat->id) + ", "
              + to_string(message->text) + ", '" + bat_id + "')");
      tx.commit();

      bot.getApi().sendMessage(message->chat->id,
                               "Added this battery to list of your batteries (check it - /my_batteries) and given it ID: "
                                   + bat_id);
      waitingForBatteryType = false;
    } else if (waitingForBattery) {
      waitingForBattery = false;
      string messageToSend;
      pqxx::work tx(*C);

      pqxx::result batteryIDresult = tx.exec(
          "select id from battery where user_id = "
              + to_string(message->chat->id)
              + " and special_bat_id = '"
              + to_string(message->text) + "'");
      currentBatteryID = string(batteryIDresult[0][0].c_str());
      pqxx::result flightType = tx.exec(
          "select * from flight_type order by id");
      for (auto row : flightType) {
        messageToSend += string(row["id"].c_str()) + ") "
            + string(row["name"].c_str()) + "\n";
      }

      bot.getApi().sendMessage(message->chat->id,
                               "Select type of flight (send a number):\n" + messageToSend);
      tx.commit();
      waitingForFlightType = true;
    } else if (waitingForFlightType) {
      currentFlightType = to_string(message->text);

      waitingForFlightType = false;
      bot.getApi().sendMessage(message->chat->id,
                               "Send amount of spent energy in mAh(send just a number, for example: 100 ):\n ");
      waitingForSpentEnergy = true;
    } else if (waitingForSpentEnergy) {
      currentSpentEnergy = to_string(message->text);
      waitingForSpentEnergy = false;
      bot.getApi().sendMessage(message->chat->id,
                               "Send time of flight(for example: 1:35)\n ");
      waitingForTime = true;
    } else if (waitingForTime) {
      waitingForTime = false;
      string spentTime = (to_string(message->text));
      int found = spentTime.find(":");
      cout << found << "\n";
      spentTime = to_string(
          stoi(spentTime.substr(0, found)) * 60
              + stoi(spentTime.substr(found + 1)));
      pqxx::work tx(*C);
      tx.exec(
          "insert into flight(battery_id, flight_type_id, spent_energy, flight_time, timestamp) values("
              + currentBatteryID + ", " + currentFlightType + ", "
              + currentSpentEnergy + "," + spentTime + "," + to_string(message->date) +")");
      tx.commit();

      bot.getApi().sendMessage(message->chat->id,
                               "Added this flight to your flights(check it - /my_flights )\n");
    } else if (waitingForBatteryRemoving) {
      work tx(*C);
      tx.exec("delete from battery where user_id = " + to_string(message->chat->id) + " and special_bat_id = '"
                  + to_string(message->text) + "'");
      bot.getApi().sendMessage(message->chat->id,
                               "Battery " + to_string(message->text)
                                   + " was removed from your list. Check - /my_batteries");
      waitingForBatteryRemoving = false;
      tx.commit();
    } else if (waitingForShowingNextFlights) {
      if (message->text == "y" && currentFlightRow != flight->size()) {
      string messageToSend;

        currentTen++;
        for (int i = currentFlightRow; i !=
            min(currentTen*FLIGTHS_IN_ONE_MESSAGE, flight->size()); i++) {
          currentFlightRow++;
          row flightRow = (*flight)[i];
          if (currentDay == 0 ||
              getDayInt(flightRow["timestamp"]) != currentDay) {
            currentDay = getDayInt(flightRow["timestamp"]);
            ostringstream dateStream;
            dateStream << sys_days {days(currentDay)};
            messageToSend += dateStream.str() + "\n";
          }
          messageToSend += outputFligthRowString(flightRow);
        }

      bot.getApi().sendMessage(message->chat->id, messageToSend);
      if (currentFlightRow < flight->size()) {
        bot.getApi().sendMessage(message->chat->id, "More? (send one letter y/n)");
        waitingForShowingNextFlights = true;
      }
      } else {
        waitingForShowingNextFlights = false;
        currentFlightRow = 0;
        currentTen = 0;
      }
    } else if (to_string(message->text)[0] != '/') {
      bot.getApi().sendMessage(message->chat->id, "Send /start");
    }

  });

  printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
  TgBot::TgLongPoll longPoll(bot);
  while (true) {
    printf("Long poll started\n");
    longPoll.start();
  }

}
