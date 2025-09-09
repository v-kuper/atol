import { useState } from 'react';
import {
  View,
  StyleSheet,
  Button,
  Text,
  ScrollView,
  SafeAreaView,
  TextInput,
  Alert,
  Platform,
} from 'react-native';
import {
  reverseString,
  getNumbers,
  getObject,
  connect,
  reconnect,
  checkConnectionStatus,
  heartbeat,
  getShiftStatus,
  openShift,
  closeShift,
  cashIncome,
  cashOutcome,
  processJson,
  sellProduct,
  setDateTime,
  disconnect,
  printXReport,
} from 'react-native-atol';

/**
 * Пояснения к обновлению:
 * - Из native-модуля убраны функции callMeLater / promiseNumber / formatEpoch (их нет в новом C++ коде).
 * - Добавлен локальный helper formatEpoch().
 * - Упрощён блок "Базовые тесты" (оставлены только поддерживаемые функции).
 * - Улучшено отображение статуса подключения (state connectionInfo).
 * - Добавлена защитная проверка наличия методов (если вдруг версия native отлична).
 */

interface TestResult {
  id: number;
  functionName: string;
  result: string;
  timestamp: string;
  isError?: boolean;
}

const safeStringify = (value: any) => {
  try {
    if (value === undefined) return 'undefined';
    if (value === null) return 'null';
    return typeof value === 'string' ? value : JSON.stringify(value, null, 2);
  } catch (e) {
    return String(value);
  }
};

// Локальная реализация formatEpoch (ms -> строка)
const formatEpoch = (ms?: number) => {
  if (!ms || ms <= 0) return '—';
  try {
    const d = new Date(ms);
    return (
      d.getFullYear() +
      '-' +
      String(d.getMonth() + 1).padStart(2, '0') +
      '-' +
      String(d.getDate()).padStart(2, '0') +
      ' ' +
      String(d.getHours()).padStart(2, '0') +
      ':' +
      String(d.getMinutes()).padStart(2, '0') +
      ':' +
      String(d.getSeconds()).padStart(2, '0')
    );
  } catch {
    return String(ms);
  }
};

export default function App() {
  const [results, setResults] = useState<TestResult[]>([]);
  const [inputText, setInputText] = useState<string>('Hello World');

  console.log(results)

  // Параметры подключения
  const [ipAddress, setIpAddress] = useState<string>('192.168.0.114');
  const [port, setPort] = useState<string>('5555');
  const [deviceName, setDeviceName] = useState<string>('АТОЛ Касса');

  // Кассовые операции
  const [cashierName, setCashierName] = useState<string>('КАССИР 1');
  const [amount, setAmount] = useState<string>('100.00');

  // JSON для операций
  const [jsonTask, setJsonTask] = useState<string>(`{
  "type": "sell",
  "items": [
    { "name": "Товар", "price": 100.00, "quantity": 1, "tax": 1 }
  ],
  "payments": [ { "type": 1, "sum": 100.00 } ]
}`);

  // Дата / время
  const [dateTimeStr, setDateTimeStr] = useState<string>('2025-01-01 10:00:00');

  // Краткая сводка подключения
  const [connectionInfo, setConnectionInfo] = useState<{
    isConnected?: boolean;
    modelName?: string;
    shiftState?: number;
    cashSum?: number;
  }>({});

  const addResult = (
    functionName: string,
    result: any,
    isError: boolean = false
  ): void => {
    const timestamp = new Date().toLocaleTimeString();
    const id = Date.now() + Math.random();
    setResults((prev) => [
      { id, functionName, result: safeStringify(result), timestamp, isError },
      ...prev,
    ]);
  };

  const updatePendingResult = (
    functionName: string,
    newValue: any,
    isError = false
  ) => {
    setResults((prev) =>
      prev.map((r) =>
        r.functionName === functionName && r.result.startsWith('...')
          ? { ...r, result: safeStringify(newValue), isError }
          : r
      )
    );
  };

  const markError = (fn: string, error: any) => {
    updatePendingResult(fn, `ERROR: ${String(error)}`, true);
  };

  const clearResults = () => setResults([]);

  // ---- Базовые тесты (оставлены только поддерживаемые) ----
  const testReverseString = () => {
    try {
      addResult('reverseString', reverseString(inputText));
    } catch (e) {
      addResult('reverseString', `ERROR: ${String(e)}`, true);
    }
  };

  const testGetNumbers = () => {
    try {
      addResult('getNumbers', getNumbers());
    } catch (e) {
      addResult('getNumbers', `ERROR: ${String(e)}`, true);
    }
  };

  const testGetObject = () => {
    try {
      addResult('getObject', getObject());
    } catch (e) {
      addResult('getObject', `ERROR: ${String(e)}`, true);
    }
  };

  // ---- Подключение ----
  const testConnect = async () => {
    if (!ipAddress.trim()) {
      Alert.alert('Ошибка', 'Введите IP адрес');
      return;
    }
    if (!port.trim() || isNaN(Number(port))) {
      Alert.alert('Ошибка', 'Введите корректный порт');
      return;
    }
    addResult('connect', '... подключение');
    try {
      const r: any = await (connect as any)(
        ipAddress.trim(),
        port.trim(),
        deviceName.trim()
      );
      updatePendingResult('connect', r);
      setConnectionInfo({
        isConnected: r?.isConnected,
        modelName: r?.modelName,
        shiftState: r?.shiftState,
        cashSum: r?.cashSum,
      });
      Alert.alert('Успех', 'Подключено');
    } catch (e) {
      markError('connect', e);
      console.log('connect', e);
      Alert.alert('Ошибка', String(e));
    }
  };

  const testReconnect = async () => {
    addResult('reconnect', '... переподключение');
    try {
      const r: any = await (reconnect as any)(
        ipAddress.trim(),
        port.trim(),
        deviceName.trim()
      );
      updatePendingResult('reconnect', r);
      setConnectionInfo({
        isConnected: r?.isConnected,
        modelName: r?.modelName,
        shiftState: r?.shiftState,
        cashSum: r?.cashSum,
      });
    } catch (e) {
      markError('reconnect', e);
    }
  };

  const testCheckConnection = async () => {
    addResult('checkConnectionStatus', '... проверка');
    try {
      const r: any = await (checkConnectionStatus as any)();
      updatePendingResult('checkConnectionStatus', r);
      setConnectionInfo((ci) => ({ ...ci, isConnected: r?.isConnected }));
    } catch (e) {
      markError('checkConnectionStatus', e);
    }
  };

  const testHeartbeat = async () => {
    addResult('heartbeat', '... отправка');
    try {
      const r: any = await (heartbeat as any)();
      updatePendingResult('heartbeat', r);
    } catch (e) {
      markError('heartbeat', e);
    }
  };

  // ---- Смена ----
  const testGetShiftStatus = async () => {
    addResult('getShiftStatus', '... запрос');
    try {
      const r: any = await (getShiftStatus as any)();
      if (r && r.dateTime) r.readableDate = formatEpoch(r.dateTime);
      updatePendingResult('getShiftStatus', r);
      setConnectionInfo((ci) => ({
        ...ci,
        shiftState: r?.shiftState,
        cashSum: r?.cashSum,
      }));
    } catch (e) {
      markError('getShiftStatus', e);
    }
  };

  const testOpenShift = async () => {
    addResult('openShift', '... открытие');
    try {
      const r: any = await (openShift as any)(cashierName.trim());
      updatePendingResult('openShift', r);
    } catch (e) {
      markError('openShift', e);
    }
  };

  const testCloseShift = async () => {
    addResult('closeShift', '... закрытие');
    try {
      const r: any = await (closeShift as any)(cashierName.trim());
      updatePendingResult('closeShift', r);
    } catch (e) {
      markError('closeShift', e);
    }
  };

  // ---- Деньги ----
  const testCashIncome = async () => {
    const val = parseFloat(amount);
    if (isNaN(val)) {
      Alert.alert('Ошибка', 'Введите корректную сумму');
      return;
    }
    addResult('cashIncome', '... внесение');
    try {
      const r: any = await (cashIncome as any)(val, cashierName.trim());
      updatePendingResult('cashIncome', r);
      if (r?.cashSum != null)
        setConnectionInfo((ci) => ({ ...ci, cashSum: r.cashSum }));
    } catch (e) {
      markError('cashIncome', e);
    }
  };

  const testCashOutcome = async () => {
    const val = parseFloat(amount);
    if (isNaN(val)) {
      Alert.alert('Ошибка', 'Введите корректную сумму');
      return;
    }
    addResult('cashOutcome', '... изъятие');
    try {
      const r: any = await (cashOutcome as any)(val, cashierName.trim());
      updatePendingResult('cashOutcome', r);
      if (r?.cashSum != null)
        setConnectionInfo((ci) => ({ ...ci, cashSum: r.cashSum }));
    } catch (e) {
      markError('cashOutcome', e);
    }
  };

  // ---- JSON ----
  const testProcessJson = async () => {
    addResult('processJson', '... обработка');
    try {
      const r: any = await (processJson as any)(jsonTask);
      updatePendingResult('processJson', r);
    } catch (e) {
      markError('processJson', e);
    }
  };

  const testSellProduct = async () => {
    addResult('sellProduct', '... продажа');
    try {
      const r: any = await (sellProduct as any)(jsonTask);
      updatePendingResult('sellProduct', r);
    } catch (e) {
      markError('sellProduct', e);
    }
  };

  // ---- Время ----
  const testSetDateTime = async () => {
    addResult('setDateTime', '... установка');
    try {
      const r: any = await (setDateTime as any)(dateTimeStr.trim());
      updatePendingResult('setDateTime', r);
    } catch (e) {
      markError('setDateTime', e);
    }
  };

  // ---- Отключение ----
  const testDisconnect = async () => {
    addResult('disconnect', '... отключение');
    try {
      const r: any = await (disconnect as any)();
      updatePendingResult('disconnect', r);
      setConnectionInfo((ci) => ({ ...ci, isConnected: false }));
    } catch (e) {
      markError('disconnect', e);
    }
  };

  const testPrintXReport = async () => {
    addResult('printXReport', '... печать');
    try {
      const r: any = await (printXReport as any)();
      updatePendingResult('printXReport', r);
    } catch (e) {
      markError('printXReport', e);
    }
  };

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView
        style={styles.scrollView}
        showsVerticalScrollIndicator={false}
        keyboardShouldPersistTaps="handled"
      >
        <View style={styles.header}>
          <Text style={styles.title}>Тестирование JSI функций</Text>
          <Text style={styles.subtitle}>react-native-atol (обновлено)</Text>
          <View style={styles.statusRow}>
            <Text style={styles.statusLabel}>Статус:</Text>
            <Text
              style={[
                styles.statusValue,
                { color: connectionInfo.isConnected ? '#4CAF50' : '#F44336' },
              ]}
            >
              {connectionInfo.isConnected ? 'Подключено' : 'Нет соединения'}
            </Text>
          </View>
          {connectionInfo.modelName && (
            <Text style={styles.statusMeta}>
              Модель: {connectionInfo.modelName}
            </Text>
          )}
          {connectionInfo.cashSum != null && (
            <Text style={styles.statusMeta}>
              Cash: {connectionInfo.cashSum}
            </Text>
          )}
        </View>

        {/* Базовые тесты */}
        <View style={styles.inputContainer}>
          <Text style={styles.sectionTitle}>🔧 Базовые тесты</Text>
          <Text style={styles.inputLabel}>Текст для reverseString:</Text>
          <TextInput
            style={styles.textInput}
            value={inputText}
            onChangeText={setInputText}
            placeholder="Введите текст..."
          />
          <View style={styles.rowButtons}>
            <Button
              title="🔄 Reverse"
              onPress={testReverseString}
              color="#2196F3"
            />
            <Button
              title="📊 Numbers"
              onPress={testGetNumbers}
              color="#4CAF50"
            />
            <Button title="📦 Object" onPress={testGetObject} color="#FF9800" />
          </View>
        </View>

        {/* Подключение */}
        <View style={styles.inputContainer}>
          <Text style={styles.sectionTitle}>🖨️ Подключение к кассе</Text>
          <Text style={styles.inputLabel}>IP адрес:</Text>
          <TextInput
            style={styles.textInput}
            value={ipAddress}
            onChangeText={setIpAddress}
            placeholder="192.168.1.100"
          />
          <Text style={styles.inputLabel}>Порт:</Text>
          <TextInput
            style={styles.textInput}
            value={port}
            onChangeText={setPort}
            placeholder="5555"
            keyboardType="numeric"
          />
          <Text style={styles.inputLabel}>Имя устройства:</Text>
          <TextInput
            style={styles.textInput}
            value={deviceName}
            onChangeText={setDeviceName}
            placeholder="АТОЛ Касса"
          />
          <View style={styles.rowButtons}>
            <Button title="🖨️ Connect" onPress={testConnect} color="#FF5722" />
            <Button
              title="🔁 Reconnect"
              onPress={testReconnect}
              color="#795548"
            />
          </View>
          <View style={styles.rowButtons}>
            <Button
              title="✅ Heartbeat"
              onPress={testHeartbeat}
              color="#3F51B5"
            />
            <Button
              title="🔍 Status"
              onPress={testCheckConnection}
              color="#607D8B"
            />
          </View>
          <View style={styles.rowButtons}>
            <Button
              title="❌ Disconnect"
              onPress={testDisconnect}
              color="#9E9E9E"
            />
            <Button
              title="🧾 X Report"
              onPress={testPrintXReport}
              color="#009688"
            />
          </View>
        </View>

        {/* Смена */}
        <View style={styles.inputContainer}>
          <Text style={styles.sectionTitle}>🕓 Смена</Text>
          <Text style={styles.inputLabel}>Имя кассира:</Text>
          <TextInput
            style={styles.textInput}
            value={cashierName}
            onChangeText={setCashierName}
            placeholder="КАССИР"
          />
          <View style={styles.rowButtons}>
            <Button title="🔓 Open" onPress={testOpenShift} color="#4CAF50" />
            <Button title="🔒 Close" onPress={testCloseShift} color="#E91E63" />
            <Button
              title="ℹ️ Status"
              onPress={testGetShiftStatus}
              color="#2196F3"
            />
          </View>
        </View>

        {/* Деньги */}
        <View style={styles.inputContainer}>
          <Text style={styles.sectionTitle}>💰 Денежные операции</Text>
          <Text style={styles.inputLabel}>Сумма:</Text>
          <TextInput
            style={styles.textInput}
            value={amount}
            onChangeText={setAmount}
            placeholder="100.00"
            keyboardType="numeric"
          />
          <View style={styles.rowButtons}>
            <Button
              title="⬆️ Income"
              onPress={testCashIncome}
              color="#8BC34A"
            />
            <Button
              title="⬇️ Outcome"
              onPress={testCashOutcome}
              color="#FF5722"
            />
          </View>
        </View>

        {/* JSON */}
        <View style={styles.inputContainer}>
          <Text style={styles.sectionTitle}>🧾 JSON операции</Text>
          <Text style={styles.inputLabel}>JSON Task:</Text>
          <TextInput
            style={[styles.textInput, styles.jsonInput]}
            value={jsonTask}
            onChangeText={setJsonTask}
            multiline
            numberOfLines={8}
            textAlignVertical="top"
          />
          <View style={styles.rowButtons}>
            <Button
              title="⚙️ processJson"
              onPress={testProcessJson}
              color="#9C27B0"
            />
            <Button
              title="🛒 sellProduct"
              onPress={testSellProduct}
              color="#673AB7"
            />
          </View>
        </View>

        {/* Время */}
        <View style={styles.inputContainer}>
          <Text style={styles.sectionTitle}>🕒 Установка времени</Text>
          <Text style={styles.inputLabel}>
            Дата/время (yyyy-MM-dd HH:mm:ss):
          </Text>
          <TextInput
            style={styles.textInput}
            value={dateTimeStr}
            onChangeText={setDateTimeStr}
            placeholder="2025-01-01 10:00:00"
          />
          <Button
            title="🗓️ setDateTime"
            onPress={testSetDateTime}
            color="#3F51B5"
          />
        </View>

        {/* Результаты */}
        <View style={styles.resultsContainer}>
          <View style={styles.resultsHeader}>
            <Text style={styles.resultsTitle}>Результаты</Text>
            <Button title="Очистить" onPress={clearResults} color="#757575" />
          </View>
          {results.length === 0 ? (
            <Text style={styles.noResults}>Нет результатов</Text>
          ) : (
            results.map((item) => (
              <View
                key={item.id}
                style={[
                  styles.resultItem,
                  item.isError && { borderLeftColor: '#F44336' },
                ]}
              >
                <View style={styles.resultHeader}>
                  <Text
                    style={[
                      styles.resultFunction,
                      item.isError && { color: '#F44336' },
                    ]}
                  >
                    {item.functionName}
                  </Text>
                  <Text style={styles.resultTime}>{item.timestamp}</Text>
                </View>
                <Text style={styles.resultText}>{item.result}</Text>
              </View>
            ))
          )}
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Стили ----
const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  scrollView: { flex: 1, paddingHorizontal: 16 },
  header: { alignItems: 'center', paddingVertical: 20, marginBottom: 12 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#333', marginBottom: 4 },
  subtitle: { fontSize: 14, color: '#666', fontStyle: 'italic' },
  statusRow: {
    flexDirection: 'row',
    gap: 6,
    marginTop: 6,
    alignItems: 'center',
  },
  statusLabel: { fontSize: 14, fontWeight: '600', color: '#444' },
  statusValue: { fontSize: 14, fontWeight: '700' },
  statusMeta: { fontSize: 12, color: '#555', marginTop: 2 },

  inputContainer: {
    backgroundColor: '#fff',
    borderRadius: 10,
    padding: 16,
    marginBottom: 18,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.07,
    shadowRadius: 4,
    elevation: 3,
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#222',
    marginBottom: 14,
    textAlign: 'center',
  },
  inputLabel: {
    fontSize: 14,
    fontWeight: '600',
    color: '#333',
    marginBottom: 6,
    marginTop: 4,
  },
  textInput: {
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 6,
    padding: 12,
    fontSize: 15,
    marginBottom: 12,
    backgroundColor: '#fafafa',
  },
  jsonInput: {
    minHeight: 160,
    fontFamily: Platform.select({
      ios: 'Menlo',
      android: 'monospace',
      default: 'monospace',
    }),
  },
  rowButtons: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    gap: 12,
    marginBottom: 12,
  },

  resultsContainer: {
    backgroundColor: '#fff',
    borderRadius: 10,
    padding: 16,
    marginBottom: 30,
    elevation: 3,
  },
  resultsHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 14,
  },
  resultsTitle: { fontSize: 18, fontWeight: 'bold', color: '#333' },
  noResults: {
    textAlign: 'center',
    color: '#999',
    fontStyle: 'italic',
    paddingVertical: 20,
  },
  resultItem: {
    backgroundColor: '#f8f9fa',
    borderRadius: 6,
    padding: 10,
    marginBottom: 10,
    borderLeftWidth: 4,
    borderLeftColor: '#2196F3',
  },
  resultHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 6,
  },
  resultFunction: { fontSize: 15, fontWeight: '600', color: '#2196F3' },
  resultTime: { fontSize: 11, color: '#666' },
  resultText: {
    fontSize: 13,
    color: '#222',
    fontFamily: Platform.select({
      ios: 'Menlo',
      android: 'monospace',
      default: 'monospace',
    }),
    backgroundColor: '#fff',
    padding: 8,
    borderRadius: 4,
    borderWidth: 1,
    borderColor: '#eee',
  },
});
