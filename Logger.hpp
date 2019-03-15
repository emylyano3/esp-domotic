#pragma once
template <class T> void debug (T text) {
  Serial.print("*DOMO: ");
  Serial.println(text);
}

template <class T, class U> void debug (T key, U value) {
  Serial.print("*DOMO: ");
  Serial.print(key);
  Serial.print(": ");
  Serial.println(value);
}