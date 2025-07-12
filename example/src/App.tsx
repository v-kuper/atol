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
} from 'react-native';
import {
  getObject,
  getNumbers,
  callMeLater,
  promiseNumber,
  reverseString,
} from 'react-native-atol';

interface TestResult {
  id: number;
  functionName: string;
  result: string;
  timestamp: string;
}

export default function App() {
  const [results, setResults] = useState<TestResult[]>([]);
  const [inputText, setInputText] = useState<string>('Hello World');
  const [inputNumber, setInputNumber] = useState<string>('5');

  const addResult = (functionName: string, result: any): void => {
    const timestamp = new Date().toLocaleTimeString();
    const id = Date.now();
    setResults((prev) => [
      {
        id,
        functionName,
        result: JSON.stringify(result, null, 2),
        timestamp,
      },
      ...prev,
    ]);
  };

  const clearResults = (): void => {
    setResults([]);
  };

  const testReverseString = (): void => {
    try {
      const result: string = reverseString(inputText);
      console.log('reverseString result:', result);
      addResult('reverseString', result);
    } catch (error) {
      console.error('reverseString error:', error);
      addResult('reverseString', `ERROR: ${(error as Error).message}`);
    }
  };

  const testGetNumbers = (): void => {
    try {
      const result: number[] = getNumbers();
      console.log('getNumbers result:', result);
      addResult('getNumbers', result);
    } catch (error) {
      console.error('getNumbers error:', error);
      addResult('getNumbers', `ERROR: ${(error as Error).message}`);
    }
  };

  const testGetObject = (): void => {
    try {
      const result = getObject();
      console.log('getObject result:', result);
      addResult('getObject', result);
    } catch (error) {
      console.error('getObject error:', error);
      addResult('getObject', `ERROR: ${(error as Error).message}`);
    }
  };

  const testPromiseNumber = async (): Promise<void> => {
    try {
      const numberValue = parseFloat(inputNumber);
      if (isNaN(numberValue)) {
        Alert.alert('Ошибка', 'Введите корректное число');
        return;
      }

      addResult('promiseNumber', 'Ожидание результата...');
      const result: number = await promiseNumber(numberValue);
      console.log('promiseNumber result:', result);

      setResults((prev) => {
        const newResults = [...prev];
        const lastIndex = newResults.findIndex(
          (r) => r.functionName === 'promiseNumber'
        );
        if (lastIndex !== -1) {
          const existingResult = newResults[lastIndex];
          newResults[lastIndex] = {
            id: existingResult!.id,
            functionName: existingResult!.functionName,
            result: JSON.stringify(result, null, 2),
            timestamp: new Date().toLocaleTimeString(),
          };
        }
        return newResults;
      });
    } catch (error) {
      console.error('promiseNumber error:', error);
      addResult('promiseNumber', `ERROR: ${(error as Error).message}`);
    }
  };

  const testCallMeLater = (): void => {
    try {
      addResult('callMeLater', 'Вызов callback функций...');
      callMeLater(
        () => {
          console.log('callMeLater success callback');
          addResult('callMeLater SUCCESS', 'Success callback выполнен');
        },
        () => {
          console.log('callMeLater failure callback');
          addResult('callMeLater FAILURE', 'Failure callback выполнен');
        }
      );
    } catch (error) {
      console.error('callMeLater error:', error);
      addResult('callMeLater', `ERROR: ${(error as Error).message}`);
    }
  };

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView
        style={styles.scrollView}
        showsVerticalScrollIndicator={false}
      >
        <View style={styles.header}>
          <Text style={styles.title}>Тестирование JSI функций</Text>
          <Text style={styles.subtitle}>react-native-atol</Text>
        </View>

        <View style={styles.inputContainer}>
          <Text style={styles.inputLabel}>Текст для reverseString:</Text>
          <TextInput
            style={styles.textInput}
            value={inputText}
            onChangeText={setInputText}
            placeholder="Введите текст..."
          />

          <Text style={styles.inputLabel}>Число для promiseNumber:</Text>
          <TextInput
            style={styles.textInput}
            value={inputNumber}
            onChangeText={setInputNumber}
            placeholder="Введите число..."
            keyboardType="numeric"
          />
        </View>

        <View style={styles.buttonContainer}>
          <Button
            title="🔄 Reverse String"
            onPress={testReverseString}
            color="#2196F3"
          />

          <Button
            title="📊 Get Numbers"
            onPress={testGetNumbers}
            color="#4CAF50"
          />

          <Button
            title="📦 Get Object"
            onPress={testGetObject}
            color="#FF9800"
          />

          <Button
            title="⏱️ Promise Number"
            onPress={testPromiseNumber}
            color="#9C27B0"
          />

          <Button
            title="📞 Call Me Later"
            onPress={testCallMeLater}
            color="#F44336"
          />
        </View>

        <View style={styles.resultsContainer}>
          <View style={styles.resultsHeader}>
            <Text style={styles.resultsTitle}>Результаты:</Text>
            <Button title="Очистить" onPress={clearResults} color="#757575" />
          </View>

          {results.length === 0 ? (
            <Text style={styles.noResults}>
              Нет результатов для отображения
            </Text>
          ) : (
            results.map((item) => (
              <View key={item.id} style={styles.resultItem}>
                <View style={styles.resultHeader}>
                  <Text style={styles.resultFunction}>{item.functionName}</Text>
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

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f5f5',
  },
  scrollView: {
    flex: 1,
    paddingHorizontal: 16,
  },
  header: {
    alignItems: 'center',
    paddingVertical: 20,
    marginBottom: 20,
  },
  title: {
    fontSize: 24,
    fontWeight: 'bold',
    color: '#333',
    marginBottom: 4,
  },
  subtitle: {
    fontSize: 16,
    color: '#666',
    fontStyle: 'italic',
  },
  inputContainer: {
    backgroundColor: '#fff',
    borderRadius: 8,
    padding: 16,
    marginBottom: 20,
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 2,
    },
    shadowOpacity: 0.1,
    shadowRadius: 3.84,
    elevation: 5,
  },
  inputLabel: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
    marginBottom: 8,
  },
  textInput: {
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 6,
    padding: 12,
    fontSize: 16,
    marginBottom: 16,
    backgroundColor: '#fafafa',
  },
  buttonContainer: {
    backgroundColor: '#fff',
    borderRadius: 8,
    padding: 16,
    marginBottom: 20,
    gap: 12,
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 2,
    },
    shadowOpacity: 0.1,
    shadowRadius: 3.84,
    elevation: 5,
  },
  resultsContainer: {
    backgroundColor: '#fff',
    borderRadius: 8,
    padding: 16,
    marginBottom: 20,
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 2,
    },
    shadowOpacity: 0.1,
    shadowRadius: 3.84,
    elevation: 5,
  },
  resultsHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 16,
  },
  resultsTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#333',
  },
  noResults: {
    textAlign: 'center',
    color: '#999',
    fontStyle: 'italic',
    paddingVertical: 20,
  },
  resultItem: {
    backgroundColor: '#f8f9fa',
    borderRadius: 6,
    padding: 12,
    marginBottom: 12,
    borderLeftWidth: 4,
    borderLeftColor: '#2196F3',
  },
  resultHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  resultFunction: {
    fontSize: 16,
    fontWeight: '600',
    color: '#2196F3',
  },
  resultTime: {
    fontSize: 12,
    color: '#666',
  },
  resultText: {
    fontSize: 14,
    color: '#333',
    fontFamily: 'monospace',
    backgroundColor: '#fff',
    padding: 8,
    borderRadius: 4,
    borderWidth: 1,
    borderColor: '#eee',
  },
});
