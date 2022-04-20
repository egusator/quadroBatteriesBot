#include <iostream>
#include <pqxx/pqxx>
#include <tgbot/tgbot.h>
#include "date.h"
#include <sstream>
using namespace std;
using namespace pqxx;
using namespace TgBot;
using namespace date;
using namespace chrono;

string token, commandsList, connInfo, currentBatteryID,
    currentSpentEnergy, currentFlightType;

bool waitingForBatteryType = false, waitingForBattery = false,
    waitingForFlightType = false, waitingForSpentEnergy = false,
    waitingForTime = false, waitingForBatteryRemoving = false,
    waitingForBatteryToShowFlights = false;
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
                                   + "\nThis bot is created to manage batteries for quadrocopters.\nList of commands:\n"
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
                              pqxx::result manufacturer = tx.exec(
                                  "SELECT * FROM battery_manufacturer");
                              pqxx::result battery_type = tx.exec(
                                  "SELECT * FROM battery_type");
                              int i = 0;
                              for (auto row : battery_type) {

                                messageToSend += string(row[0].c_str()) + ") ";

                                for (auto manufRow : manufacturer) {
                                  if (row[1] == manufRow[0]) {
                                    string c = string(manufRow[1].c_str());
                                    messageToSend += c + " ";

                                  }
                                }

                                if (!string(row[2].c_str()).empty())
                                  messageToSend += string(row[2].c_str()) + " ";

                                if (!string(row[3].c_str()).empty())
                                  messageToSend += string(row[3].c_str()) + " mAh ";

                                if (!string(row[4].c_str()).empty())
                                  messageToSend += "max: "
                                      + to_string(
                                          ((double) stoi(string(row[4].c_str())))
                                              / 100).substr(0, 4) + "V ";

                                if (!string(row[5].c_str()).empty())
                                  messageToSend += string(row[5].c_str()) + " ";

                                if (!string(row[6].c_str()).empty())
                                  messageToSend += "max: "
                                      + to_string(
                                          ((double) stoi(string(row[6].c_str())))
                                              / 100) + "V ";

                                if (!string(row[7].c_str()).empty())
                                  messageToSend += to_string(
                                      ((double) stoi(string(row[7].c_str()))) / 100)
                                      + "Wh \n";
                                tx.commit();
                              }
                              bot.getApi().sendMessage(message->chat->id, messageToSend);
                              waitingForBatteryType = true;
                            });

  bot.getEvents().onCommand("my_batteries",
                            [&bot](const TgBot::Message::Ptr message) {
                              string messageToSend = "List of your batteries: \n";
                              pqxx::work tx(*C);
                              pqxx::result batteryOfUser =
                                  tx.exec(
                                      "SELECT battery_type_id, special_bat_id \
							FROM battery where user_id = "
                                          + to_string(message->chat->id)
                                          + " order by special_bat_id");
                              if (batteryOfUser.size() == 0) {
                                messageToSend +=
                                    "You don't have any batteries :(\nAdd some with command /add_battery";
                              } else {
                                int i = 0;
                                for (auto row : batteryOfUser) {
                                  i++;
                                  messageToSend += string(to_string(i)) + ") "
                                      + string(row[1].c_str()) + " - ";

                                  pqxx::result battery_type =
                                      tx.exec(
                                          "SELECT manufacturer_id, name, \
								capacity, max_voltage from battery_type where id = "
                                              + string(row[0].c_str()));
                                  for (auto batteryTypeRow : battery_type) {

                                    pqxx::result manuf =
                                        tx.exec(
                                            "select name from battery_manufacturer where id = "
                                                + string(
                                                    batteryTypeRow[0].c_str()));
                                    messageToSend += string(manuf[0][0].c_str()) + " ";
                                    if (!string(batteryTypeRow[1].c_str()).empty())
                                      messageToSend += string(
                                          batteryTypeRow[1].c_str()) + " ";

                                    if (!string(batteryTypeRow[2].c_str()).empty())
                                      messageToSend += string(
                                          batteryTypeRow[2].c_str()) + " mAh ";

                                    if (!string(batteryTypeRow[3].c_str()).empty())
                                      messageToSend +=
                                          "max: "
                                              + to_string(
                                                  ((double) stoi(
                                                      string(
                                                          batteryTypeRow[3].c_str())))
                                                      / 100).substr(0,
                                                                    4) + "V \n";

                                  }
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
                                  tx.exec(
                                      "SELECT battery_type_id, special_bat_id \
											FROM battery where user_id = "
                                          + to_string(message->chat->id)
                                          + " order by special_bat_id");
                              if (batteryOfUser.size() == 0) {
                                messageToSend +=
                                    "You don't have any batteries :(\nAdd some with command /add_battery";
                              } else {
                                int i = 0;
                                for (auto row : batteryOfUser) {
                                  i++;
                                  messageToSend += string(to_string(i)) + ") "
                                      + string(row[1].c_str()) + " - ";

                                  pqxx::result battery_type =
                                      tx.exec(
                                          "SELECT manufacturer_id, name, \
												capacity, max_voltage from battery_type where id = "
                                              + string(row[0].c_str()));
                                  for (auto batteryTypeRow : battery_type) {

                                    pqxx::result manuf =
                                        tx.exec(
                                            "select name from battery_manufacturer where id = "
                                                + string(
                                                    batteryTypeRow[0].c_str()));
                                    messageToSend += string(manuf[0][0].c_str()) + " ";
                                    if (!string(batteryTypeRow[1].c_str()).empty())
                                      messageToSend += string(
                                          batteryTypeRow[1].c_str()) + " ";

                                    if (!string(batteryTypeRow[2].c_str()).empty())
                                      messageToSend += string(
                                          batteryTypeRow[2].c_str()) + " mAh ";

                                    if (!string(batteryTypeRow[3].c_str()).empty())
                                      messageToSend +=
                                          "max: "
                                              + to_string(
                                                  ((double) stoi(
                                                      string(
                                                          batteryTypeRow[3].c_str())))
                                                      / 100).substr(0,
                                                                    4) + "V \n";

                                  }
                                }

                              }

                              bot.getApi().sendMessage(message->chat->id, messageToSend);
                              waitingForBattery = true;
                            });

  bot.getEvents().onCommand("my_flights",
                            [&bot](TgBot::Message::Ptr message) {
                              string messageToSend = "All your flights:\n";
                              pqxx::work tx(*C);
                              pqxx::result flight =
                                  tx.exec(
                                      "select * from flight left join battery on flight.battery_id = battery.id \
                                              left join flight_type on flight.flight_type_id = flight_type.id \
                                      where user_id =" + to_string(message->chat->id)
                                      + "order by timestamp desc");
                              if (flight.size() == 0) {
                                sys_days ();
                                bot.getApi().sendMessage(message->chat->id, "You haven't any flights (add_some - /add_flight)");
                              } else {

                                uint64_t currentDay = 0, i = 0;
                                for (auto flightRow:flight) {

                                if (currentDay == 0 || ((stoi(string(flightRow[5].c_str()))+7*3600)/86400) != currentDay) {
                                  currentDay = (stoi(string(flightRow[5].c_str()))+7*3600)/86400;
                                  ostringstream dateStream;
                                  dateStream << sys_days {days(currentDay)};
                                  messageToSend += dateStream.str() + "\n";
                                }
                                messageToSend += to_string(++i) +") ";
                                messageToSend += string(flightRow[9].c_str()) + " - ";
                                messageToSend += string(flightRow[11].c_str()) + ", ";
                                messageToSend += "spent " + string(flightRow[3].c_str()) + " mAh, ";
                                int seconds = stoi(string(flightRow[4].c_str()));
                                messageToSend += to_string(seconds / 60) + ":"
                                      + to_string(seconds % 60) + "\n";
                                }
                              }
                              bot.getApi().sendMessage(message->chat->id, messageToSend);
  });


  bot.getEvents().onCommand("remove_battery",
                            [&bot](TgBot::Message::Ptr message) {
                              string messageToSend =
                                  "Select a battery to delete from your list(send id, for example: G1): \n";
                              pqxx::work tx(*C);
                              pqxx::result batteryOfUser =
                                  tx.exec(
                                      "SELECT battery_type_id, special_bat_id \
									FROM battery where user_id = "
                                          + to_string(message->chat->id)
                                          + " order by special_bat_id");
                              if (batteryOfUser.size() == 0) {
                                messageToSend +=
                                    "You don't have any batteries :(\nAdd some with command /add_battery";
                              } else {
                                int i = 0;
                                for (auto row : batteryOfUser) {
                                  i++;
                                  messageToSend += string(to_string(i)) + ") "
                                      + string(row[1].c_str()) + " - ";

                                  pqxx::result battery_type =
                                      tx.exec(
                                          "SELECT manufacturer_id, name, \
										capacity, max_voltage from battery_type where id = "
                                              + string(row[0].c_str()));
                                  for (auto batteryTypeRow : battery_type) {

                                    pqxx::result manuf =
                                        tx.exec(
                                            "select name from battery_manufacturer where id = "
                                                + string(
                                                    batteryTypeRow[0].c_str()));
                                    messageToSend += string(manuf[0][0].c_str()) + " ";
                                    if (!string(batteryTypeRow[1].c_str()).empty())
                                      messageToSend += string(
                                          batteryTypeRow[1].c_str()) + " ";

                                    if (!string(batteryTypeRow[2].c_str()).empty())
                                      messageToSend += string(
                                          batteryTypeRow[2].c_str()) + " mAh ";

                                    if (!string(batteryTypeRow[3].c_str()).empty())
                                      messageToSend +=
                                          "max: "
                                              + to_string(
                                                  ((double) stoi(
                                                      string(
                                                          batteryTypeRow[3].c_str())))
                                                      / 100).substr(0,
                                                                    4) + "V \n";

                                  }
                                }
                              }

                              bot.getApi().sendMessage(message->chat->id, messageToSend);
                              waitingForBatteryRemoving = true;
                            });

  bot.getEvents().onAnyMessage([&bot](TgBot::Message::Ptr message) {
    if (waitingForBatteryType) {
      pqxx::work tx(*C);
      pqxx::result r = tx.exec("SELECT * FROM battery where user_id = " + to_string(message->chat->id)
                                   + " AND battery_type_id = "
                                   + to_string(message->text));
      pqxx::result manuf = tx.exec("SELECT * FROM battery_manufacturer");

      char currentSym;
      for (auto row : manuf) {
        if (to_string(message->text) == string(row[0].c_str())) {
          currentSym = string(row[1].c_str())[0];
          break;
        }
      }
      int number = r.size() + 1;
      string bat_id = currentSym + to_string(number);
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
        messageToSend += string(row[0].c_str()) + ") "
            + string(row[1].c_str()) + "\n";
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
