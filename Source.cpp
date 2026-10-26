#include <iostream>
#include <vector>
#include <cstdlib>
#include <windows.h>

struct MarkerData {
    int id;
    int array_size;
    int* arr;
    CRITICAL_SECTION* cs;
    HANDLE hStartEvent;
    HANDLE hStopWorkerEvent;
    HANDLE hResumeEvent;
    HANDLE hExitEvent;
    bool is_alive = true;
};

DWORD WINAPI marker_thread(LPVOID param) {
    MarkerData* data = reinterpret_cast<MarkerData*>(param);

    WaitForSingleObject(data->hStartEvent, INFINITE);
    std::srand(data->id);

    int marked_count = 0;

    while (data->is_alive) {
        int idx = std::rand() % data->array_size;

        EnterCriticalSection(data->cs);
        if (data->arr[idx] == 0) {
            Sleep(5);
            data->arr[idx] = data->id;
            Sleep(5);
            LeaveCriticalSection(data->cs);
            marked_count++;
        }
        else {
            std::cout << "Поток " << data->id << ": помечено элементов = " << marked_count
                << ", невозможно пометить индекс = " << idx << "\n";
            LeaveCriticalSection(data->cs);

            SetEvent(data->hStopWorkerEvent);

            HANDLE wait_handles[2] = { data->hResumeEvent, data->hExitEvent };
            DWORD wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);

            if (wait_result == WAIT_OBJECT_0 + 1) {
                EnterCriticalSection(data->cs);
                for (int i = 0; i < data->array_size; i++) {
                    if (data->arr[i] == data->id) {
                        data->arr[i] = 0;
                    }
                }
                LeaveCriticalSection(data->cs);
                data->is_alive = false;
            }
            else {
                ResetEvent(data->hResumeEvent);
            }
        }
    }
    return 0;
}

int main() {
    system("chcp 65001 > nul");

    int array_size;
    std::cout << "Введите размерность массива: ";
    std::cin >> array_size;

    std::vector<int> arr(array_size, 0);

    int num_markers;
    std::cout << "Введите количество потоков marker: ";
    std::cin >> num_markers;

    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);

    HANDLE hStartAll = CreateEvent(NULL, TRUE, FALSE, NULL);

    std::vector<MarkerData> markers(num_markers);
    std::vector<HANDLE> hThreads(num_markers);
    std::vector<HANDLE> hStopWorkerEvents(num_markers);

    for (int i = 0; i < num_markers; i++) {
        markers[i].id = i + 1;
        markers[i].array_size = array_size;
        markers[i].arr = arr.data();
        markers[i].cs = &cs;
        markers[i].hStartEvent = hStartAll;
        markers[i].hStopWorkerEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        markers[i].hResumeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        markers[i].hExitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        hStopWorkerEvents[i] = markers[i].hStopWorkerEvent;
        hThreads[i] = CreateThread(NULL, 0, marker_thread, &markers[i], 0, NULL);
    }

    SetEvent(hStartAll);

    int active_markers = num_markers;
    while (active_markers > 0) {
        WaitForMultipleObjects(markers.size(), hStopWorkerEvents.data(), TRUE, INFINITE);

        std::cout << "Массив после остановки всех маркеров:\n";
        for (int i = 0; i < array_size; i++) {
            std::cout << arr[i] << " ";
        }
        std::cout << "\n";

        int target_id;
        while (true) {
            std::cout << "Введите номер потока для завершения (1.." << num_markers << "): ";
            std::cin >> target_id;
            if (target_id >= 1 && target_id <= num_markers && markers[target_id - 1].is_alive) {
                break;
            }
        }

        int idx = target_id - 1;
        SetEvent(markers[idx].hExitEvent);
        WaitForSingleObject(hThreads[idx], INFINITE);

        CloseHandle(hThreads[idx]);
        CloseHandle(markers[idx].hStopWorkerEvent);
        CloseHandle(markers[idx].hResumeEvent);
        CloseHandle(markers[idx].hExitEvent);

        hStopWorkerEvents[idx] = CreateEvent(NULL, TRUE, TRUE, NULL);

        std::cout << "Массив после очистки потока " << target_id << ":\n";
        for (int i = 0; i < array_size; i++) {
            std::cout << arr[i] << " ";
        }
        std::cout << "\n";

        for (int i = 0; i < num_markers; i++) {
            if (markers[i].is_alive && i != idx) {
                SetEvent(markers[i].hResumeEvent);
            }
        }

        active_markers--;
    }

    CloseHandle(hStartAll);
    DeleteCriticalSection(&cs);
    return 0;
}
